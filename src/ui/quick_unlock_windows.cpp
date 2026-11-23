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
#include <QGuiApplication>
#include <QPointer>
#include <QWindow>
#include "settings_files.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Security.Credentials.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <future>
#include <memory>
#include <optional>
#include <mutex>
#include <thread>

// --- Version ------------------------------------------------------------
// quick_unlock_windows.cpp 0.2.53

// --- Version history ----------------------------------------------------
// v0.2.53: Windows Hello opens over Chrono Vault where Windows can (the owner)
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
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

// Windows Hello opens over this window where Windows can say so: the calls
// that take an owner window and the function that names one are asked for
// while running, so an older Windows keeps the calls without one.
std::optional<winrt::Windows::UI::WindowId> ownerOf(quintptr window)
{
    if (window == 0) {
        return std::nullopt;
    }
    using Metadata = winrt::Windows::Foundation::Metadata::ApiInformation;
    try {
        if (!Metadata::IsMethodPresent(L"Windows.Security.Credentials.KeyCredentialManager",
                                       L"RequestCreateForWindowAsync") ||
            !Metadata::IsMethodPresent(L"Windows.Security.Credentials.KeyCredential", L"RequestSignForWindowAsync")) {
            return std::nullopt;
        }
    } catch (const winrt::hresult_error&) {
        return std::nullopt;
    }
    using Naming = HRESULT(WINAPI*)(HWND, winrt::Windows::UI::WindowId*);
    static const Naming naming = []() -> Naming {
        const HMODULE library = LoadLibraryExW(L"ext-ms-win-windowing-external-l1-1-0.dll", nullptr,
                                               LOAD_LIBRARY_SEARCH_SYSTEM32);
        return library != nullptr
                   ? reinterpret_cast<Naming>(reinterpret_cast<void*>(GetProcAddress(library, "GetWindowIdFromWindow")))
                   : nullptr;
    }();
    winrt::Windows::UI::WindowId id{};
    if (naming == nullptr || FAILED(naming(reinterpret_cast<HWND>(window), &id))) {
        return std::nullopt;
    }
    return id;
}

// The Chrono Vault window that has the keyboard now, on the window's thread.
quintptr ownerWindow()
{
    const QWindow* window = QGuiApplication::focusWindow();
    return window != nullptr ? static_cast<quintptr>(window->winId()) : 0;
}

struct Signed
{
    QuickUnlock::Result result = QuickUnlock::Result::Failed;
    QByteArray key; // SHA-256 of the signature
};

// Runs on a thread of its own: WinRT's waiting calls may not run on the
// window's thread.
Signed sign(const QString& vault, const QByteArray& challenge, bool create, quintptr window, Pending& pending)
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    Signed answer;
    try {
        const std::optional<winrt::Windows::UI::WindowId> owner = ownerOf(window);
        const winrt::hstring name(credentialName(vault).toStdWString());
        const credentials::KeyCredentialRetrievalResult found =
            !create  ? pending.wait(credentials::KeyCredentialManager::OpenAsync(name))
            : owner ? pending.wait(credentials::KeyCredentialManager::RequestCreateForWindowAsync(
                          *owner, name, credentials::KeyCredentialCreationOption::ReplaceExisting))
                    : pending.wait(credentials::KeyCredentialManager::RequestCreateAsync(
                          name, credentials::KeyCredentialCreationOption::ReplaceExisting));
        switch (found.Status()) {
        case credentials::KeyCredentialStatus::Success: break;
        case credentials::KeyCredentialStatus::UserCanceled: answer.result = QuickUnlock::Result::Cancelled; return answer;
        case credentials::KeyCredentialStatus::NotFound: answer.result = QuickUnlock::Result::NoKey; return answer;
        default: answer.result = QuickUnlock::Result::Failed; return answer;
        }
        const auto buffer = cryptography::CryptographicBuffer::CreateFromByteArray(winrt::array_view<uint8_t const>(
            reinterpret_cast<const uint8_t*>(challenge.constData()),
            reinterpret_cast<const uint8_t*>(challenge.constData()) + challenge.size()));
        const credentials::KeyCredentialOperationResult signedChallenge =
            owner ? pending.wait(found.Credential().RequestSignForWindowAsync(*owner, buffer))
                  : pending.wait(found.Credential().RequestSignAsync(buffer));
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
    [[nodiscard]] QString touchPrompt() const override
    {
        return QCoreApplication::translate("QuickUnlock", "Confirm it is you in the Windows Hello window…");
    }

    void store(const QString& vault, const QByteArray& secret) override
    {
        const QByteArray challenge = crypto::randomBytes(32);
        QPointer<WindowsQuickUnlock> self(this);
        // Its Cancel stops it too (D-042); a stopped one answers Cancelled.
        cancel();
        auto pending = std::make_shared<Pending>();
        asking = pending;
        const quintptr window = ownerWindow();
        emit fingerReady(); // Windows Hello shows its own window now
        std::thread([self, vault, secret, challenge, window, pending]() {
            const Signed answer = sign(vault, challenge, true, window, *pending);
            bool stopped = false;
            {
                const std::lock_guard<std::mutex> guard(pending->lock);
                stopped = pending->stopped;
            }
            QByteArray sealed;
            if (answer.result == Result::Done && !stopped) {
                sealed = crypto::seal(answer.key, secret, sealLabel);
            }
            const Result result = stopped                         ? Result::Cancelled
                                  : answer.result != Result::Done ? answer.result
                                  : sealed.isEmpty()              ? Result::NotKept
                                                                  : Result::Done;
            QMetaObject::invokeMethod(
                qApp,
                [self, vault, challenge, sealed, result]() {
                    if (!self) {
                        return;
                    }
                    if (!sealed.isEmpty()) {
                        VaultsFile().setValue(settingsKey(vault),
                                             QStringList{QString::fromLatin1(challenge.toBase64()),
                                                         QString::fromLatin1(sealed.toBase64())});
                    }
                    self->remember(vault, !sealed.isEmpty());
                    emit self->stored(result);
                },
                Qt::QueuedConnection);
        }).detach();
    }

    void retrieve(const QString& vault) override
    {
        const QStringList kept = VaultsFile().value(settingsKey(vault)).toStringList();
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
        const quintptr window = ownerWindow();
        emit fingerReady(); // Windows Hello shows its own window now
        std::thread([self, vault, challenge, sealed, window, pending]() {
            const Signed answer = sign(vault, challenge, false, window, *pending);
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
        VaultsFile().remove(settingsKey(vault));
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
