// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg <RobVandenberg64@gmail.com>
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License version 3 as
// published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
// General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
#include "quick_unlock.h"

#include "crypto.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QPointer>
#include <QSettings>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Credentials.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>

#include <future>
#include <memory>
#include <mutex>
#include <thread>

// --- Version ------------------------------------------------------------
// quick_unlock_windows.cpp 0.0.23

// --- Version history ----------------------------------------------------
// v0.0.23: Initial release (D-038)

// Windows Hello (D-038): a key credential per vault, made and used only after
// the person's fingerprint, face or PIN. It signs a random challenge kept in
// the settings; the SHA-256 of that signature is the key that seals the
// vault's key. Without Windows Hello there is no signature, and nothing to
// open the seal with.

namespace chrono_vault {

namespace {

namespace credentials = winrt::Windows::Security::Credentials;
namespace cryptography = winrt::Windows::Security::Cryptography;

const QByteArray sealLabel = QByteArrayLiteral("chrono-vault quick unlock");

QString credentialName(const QString& vault)
{
    return QStringLiteral("Chrono Vault ") +
           QString::fromLatin1(QCryptographicHash::hash(vault.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
}

QString settingsKey(const QString& vault)
{
    return QStringLiteral("quickUnlock/windows/") + credentialName(vault).section(QLatin1Char(' '), -1);
}

// The Windows Hello request a check is waiting for, so that it can be
// stopped when the vault is unlocked another way.
struct Pending
{
    std::mutex lock;
    bool stopped = false;
    winrt::Windows::Foundation::IAsyncInfo request{nullptr};

    template <typename Operation> auto wait(Operation operation)
    {
        {
            const std::lock_guard<std::mutex> guard(lock);
            request = operation;
            if (stopped) {
                operation.Cancel();
            }
        }
        auto answer = operation.get(); // a stopped request throws
        const std::lock_guard<std::mutex> guard(lock);
        request = nullptr;
        return answer;
    }

    void stop()
    {
        const std::lock_guard<std::mutex> guard(lock);
        stopped = true;
        if (request) {
            request.Cancel();
        }
    }
};

struct Signed
{
    QuickUnlock::Result result = QuickUnlock::Result::Failed;
    QByteArray key; // SHA-256 of the signature
};

// Runs on a thread of its own: WinRT's waiting calls may not run on the
// window's thread.
Signed sign(const QString& vault, const QByteArray& challenge, bool create, Pending& pending)
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    Signed answer;
    try {
        const winrt::hstring name(credentialName(vault).toStdWString());
        const credentials::KeyCredentialRetrievalResult found =
            create ? pending.wait(credentials::KeyCredentialManager::RequestCreateAsync(
                         name, credentials::KeyCredentialCreationOption::ReplaceExisting))
                   : pending.wait(credentials::KeyCredentialManager::OpenAsync(name));
        switch (found.Status()) {
        case credentials::KeyCredentialStatus::Success: break;
        case credentials::KeyCredentialStatus::UserCanceled: answer.result = QuickUnlock::Result::Cancelled; return answer;
        case credentials::KeyCredentialStatus::NotFound: answer.result = QuickUnlock::Result::NoKey; return answer;
        default: answer.result = QuickUnlock::Result::Failed; return answer;
        }
        const auto buffer = cryptography::CryptographicBuffer::CreateFromByteArray(winrt::array_view<uint8_t const>(
            reinterpret_cast<const uint8_t*>(challenge.constData()),
            reinterpret_cast<const uint8_t*>(challenge.constData()) + challenge.size()));
        const credentials::KeyCredentialOperationResult signedChallenge = pending.wait(found.Credential().RequestSignAsync(buffer));
        if (signedChallenge.Status() == credentials::KeyCredentialStatus::UserCanceled) {
            answer.result = QuickUnlock::Result::Cancelled;
            return answer;
        }
        if (signedChallenge.Status() != credentials::KeyCredentialStatus::Success) {
            return answer;
        }
        winrt::com_array<uint8_t> signature;
        cryptography::CryptographicBuffer::CopyToByteArray(signedChallenge.Result(), signature);
        answer.key = QCryptographicHash::hash(
            QByteArray(reinterpret_cast<const char*>(signature.data()), static_cast<qsizetype>(signature.size())),
            QCryptographicHash::Sha256);
        answer.result = QuickUnlock::Result::Done;
    } catch (const winrt::hresult_error&) {
        answer.result = QuickUnlock::Result::Unavailable;
    }
    return answer;
}

class WindowsQuickUnlock : public QuickUnlock
{
public:
    [[nodiscard]] bool available() const override
    {
        return std::async(std::launch::async, []() {
                   winrt::init_apartment(winrt::apartment_type::multi_threaded);
                   try {
                       return credentials::KeyCredentialManager::IsSupportedAsync().get();
                   } catch (const winrt::hresult_error&) {
                       return false;
                   }
               })
            .get();
    }

    [[nodiscard]] QString name() const override { return QCoreApplication::translate("QuickUnlock", "Windows Hello"); }

    void store(const QString& vault, const QByteArray& secret) override
    {
        const QByteArray challenge = crypto::randomBytes(32);
        QPointer<WindowsQuickUnlock> self(this);
        std::thread([self, vault, secret, challenge]() {
            Pending own; // turning it on is never stopped by an unlock
            const Signed answer = sign(vault, challenge, true, own);
            QByteArray sealed;
            if (answer.result == Result::Done) {
                sealed = crypto::seal(answer.key, secret, sealLabel);
            }
            QMetaObject::invokeMethod(
                qApp,
                [self, vault, challenge, sealed]() {
                    if (!self) {
                        return;
                    }
                    if (!sealed.isEmpty()) {
                        QSettings().setValue(settingsKey(vault),
                                             QStringList{QString::fromLatin1(challenge.toBase64()),
                                                         QString::fromLatin1(sealed.toBase64())});
                    }
                    self->remember(vault, !sealed.isEmpty());
                    emit self->stored(!sealed.isEmpty());
                },
                Qt::QueuedConnection);
        }).detach();
    }

    void retrieve(const QString& vault) override
    {
        const QStringList kept = QSettings().value(settingsKey(vault)).toStringList();
        if (kept.size() != 2) {
            emit retrieved(Result::NoKey, {});
            return;
        }
        const QByteArray challenge = QByteArray::fromBase64(kept.at(0).toLatin1());
        const QByteArray sealed = QByteArray::fromBase64(kept.at(1).toLatin1());
        QPointer<WindowsQuickUnlock> self(this);
        cancel();
        auto pending = std::make_shared<Pending>();
        asking = pending;
        std::thread([self, vault, challenge, sealed, pending]() {
            const Signed answer = sign(vault, challenge, false, *pending);
            {
                const std::lock_guard<std::mutex> guard(pending->lock);
                if (pending->stopped) {
                    return; // stopped: nobody waits for its answer
                }
            }
            Result result = answer.result;
            QByteArray secret;
            if (result == Result::Done) {
                const auto opened = crypto::open(answer.key, sealed, sealLabel);
                if (opened) {
                    secret = *opened;
                } else {
                    result = Result::NoKey; // Windows Hello was set up again: its key is another
                }
            }
            QMetaObject::invokeMethod(
                qApp,
                [self, result, secret]() {
                    if (self) {
                        emit self->retrieved(result, secret);
                    }
                },
                Qt::QueuedConnection);
        }).detach();
    }

    // A Windows Hello prompt still open is closed: the vault was unlocked
    // with its master password meanwhile.
    void cancel() override
    {
        if (const auto pending = std::exchange(asking, nullptr)) {
            pending->stop();
        }
    }

    void remove(const QString& vault) override
    {
        QSettings().remove(settingsKey(vault));
        remember(vault, false);
        std::thread([vault]() {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            try {
                credentials::KeyCredentialManager::DeleteAsync(winrt::hstring(credentialName(vault).toStdWString())).get();
            } catch (const winrt::hresult_error&) {
            }
        }).detach();
    }

private:
    std::shared_ptr<Pending> asking; // the check now asking for the person
};

} // namespace

std::unique_ptr<QuickUnlock> makeQuickUnlock()
{
    return std::make_unique<WindowsQuickUnlock>();
}

} // namespace chrono_vault
