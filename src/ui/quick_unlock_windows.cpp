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
#include "settings_files.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QPointer>
#include <QWindow>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dpapi.h>
#include <userconsentverifierinterop.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Credentials.h>
#include <winrt/Windows.Security.Credentials.UI.h>

#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

// --- Version ------------------------------------------------------------
// quick_unlock_windows.cpp 0.5.80

// --- Version history ----------------------------------------------------
// v0.5.80: Every thread that talks to Windows Hello closes down its COM set-up when it ends (D-055)
// v0.4.74: Windows Hello's own window brought to the front when Windows shows it (the owner)
// v0.4.73: Windows Hello always given the window that asks (the owner)
// v0.4.72.1: Debug lines: the window Windows Hello is given, the one in front, the call used (the owner)
// v0.4.72: Windows Hello asked over Chrono Vault's window on every Windows; the key kept with DPAPI (the owner, B)
// v0.2.53: Windows Hello opens over Chrono Vault where Windows can (the owner)
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.23: Initial release (D-038)

// Windows Hello (D-038 point 11, the owner's choice B): the person's
// fingerprint, face or PIN is asked through the call that takes Chrono
// Vault's window, so Windows shows it on top on every Windows version. Once
// Windows Hello says it is them, the vault's key is opened with Windows' own
// protection for this Windows account (DPAPI), as on Linux the keyring gives
// it after the finger (Q39).

namespace chrono_vault {

namespace {

namespace credentials = winrt::Windows::Security::Credentials;
namespace consent = winrt::Windows::Security::Credentials::UI;

QString vaultId(const QString& vault)
{
    return QString::fromLatin1(QCryptographicHash::hash(vault.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
}

QString settingsKey(const QString& vault)
{
    return QStringLiteral("quickUnlock/windowsUser/") + vaultId(vault);
}

// Where versions before 0.4.72 kept a key signed by a Windows Hello
// credential; removed with that credential when the vault forgets its key.
QString oldSettingsKey(const QString& vault)
{
    return QStringLiteral("quickUnlock/windows/") + vaultId(vault);
}

QString oldCredentialName(const QString& vault)
{
    return QStringLiteral("Chrono Vault ") + vaultId(vault);
}

// COM as a thread that talks to Windows Hello needs it, closed down when the
// thread is done. A thread that set it up and just ended left Explorer's file
// dialog hanging afterwards (measured, D-055). Made before any Windows object
// of the thread, so it goes after the last of them.
class HelloThread
{
public:
    HelloThread() { winrt::init_apartment(winrt::apartment_type::multi_threaded); }
    ~HelloThread()
    {
        winrt::clear_factory_cache();
        winrt::uninit_apartment();
    }
    HelloThread(const HelloThread&) = delete;
    HelloThread& operator=(const HelloThread&) = delete;
};

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
        // Let go of on every way out, a stopped request's throw too: the
        // thread closes COM down next (HelloThread).
        struct LetGo
        {
            Pending& pending;
            ~LetGo()
            {
                const std::lock_guard<std::mutex> guard(pending.lock);
                pending.request = nullptr;
            }
        } letGo{*this};
        return operation.get();
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

// The Chrono Vault window that asks, on the window's thread: the one with the
// keyboard, else the main window that is shown (no window has the keyboard
// right at a start, 0.4.72.1's log), so Windows Hello always has an owner.
quintptr ownerWindow()
{
    const QWindow* window = QGuiApplication::focusWindow();
    if (window == nullptr) {
        for (QWindow* shown : QGuiApplication::topLevelWindows()) {
            if (shown->isVisible() && shown->transientParent() == nullptr) {
                window = shown;
                break;
            }
        }
    }
    return window != nullptr ? static_cast<quintptr>(window->winId()) : 0;
}

QString titleOf(HWND window)
{
    wchar_t text[256] = {};
    GetWindowTextW(window, text, 256);
    return QString::fromWCharArray(text);
}

// Windows 10 puts Windows Hello's own window (class "Credential Dialog Xaml
// Host", made by CredentialUIBroker.exe) behind Chrono Vault even with an
// owner, and offers no call to change that. So, only while Chrono Vault itself
// asks, Windows reports each window that is shown, and that one window is
// brought to the front: no timer, no looking again (the owner's exception to
// G-3, 0.4.74; D-038 point 12).
HWINEVENTHOOK helloHook = nullptr;

bool isHelloWindow(HWND window)
{
    wchar_t className[64] = {};
    if (GetClassNameW(window, className, 64) == 0 || lstrcmpiW(className, L"Credential Dialog Xaml Host") != 0) {
        return false;
    }
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr) {
        return false;
    }
    wchar_t image[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    const bool named = QueryFullProcessImageNameW(process, 0, image, &size) != 0;
    CloseHandle(process);
    if (!named) {
        return false;
    }
    const wchar_t* name = image;
    for (const wchar_t* at = image; *at != L'\0'; ++at) {
        if (*at == L'\\') {
            name = at + 1;
        }
    }
    return lstrcmpiW(name, L"CredentialUIBroker.exe") == 0;
}

void CALLBACK helloShown(HWINEVENTHOOK, DWORD, HWND window, LONG object, LONG child, DWORD, DWORD)
{
    if (window == nullptr || object != OBJID_WINDOW || child != CHILDID_SELF || !isHelloWindow(window)) {
        return;
    }
    const bool front = SetForegroundWindow(window) != 0;
    fingerLog(QStringLiteral("hello window 0x%1 shown, brought to the front: %2")
                  .arg(reinterpret_cast<quintptr>(window), 0, 16)
                  .arg(front ? QStringLiteral("yes") : QStringLiteral("no")));
}

// On the window's thread, whose messages carry the reports.
void startBringingForward()
{
    if (helloHook == nullptr) {
        helloHook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr, helloShown, 0, 0,
                                    WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        if (helloHook == nullptr) {
            fingerLog(QStringLiteral("hello: Windows did not agree to report shown windows"));
        }
    }
}

void stopBringingForward()
{
    if (helloHook != nullptr) {
        UnhookWinEvent(helloHook);
        helloHook = nullptr;
    }
}

// For the --debug log (0.4.72.1): which window Windows Hello is given, which
// window is in front, and which call is used.
void logRequest(const char* what, quintptr window)
{
    const QWindow* focused = QGuiApplication::focusWindow();
    const HWND front = GetForegroundWindow();
    DWORD frontProcess = 0;
    GetWindowThreadProcessId(front, &frontProcess);
    fingerLog(QStringLiteral("hello %1: owner 0x%2 (Qt focus window: %3), in front 0x%4 \"%5\" (%6), call %7")
                  .arg(QLatin1String(what))
                  .arg(window, 0, 16)
                  .arg(focused != nullptr ? QStringLiteral("\"%1\"").arg(focused->title()) : QStringLiteral("none"))
                  .arg(reinterpret_cast<quintptr>(front), 0, 16)
                  .arg(titleOf(front))
                  .arg(frontProcess == GetCurrentProcessId() ? QStringLiteral("ours") : QStringLiteral("another program"))
                  .arg(window != 0 ? QStringLiteral("RequestVerificationForWindowAsync")
                                   : QStringLiteral("RequestVerificationAsync without an owner")));
}

// Asks Windows Hello whether it is the person, over the given window. Runs on
// a thread of its own: WinRT's waiting calls may not run on the window's
// thread.
QuickUnlock::Result verify(quintptr window, const QString& message, Pending& pending)
{
    const HelloThread com;
    try {
        const winrt::hstring text(message.toStdWString());
        winrt::Windows::Foundation::IAsyncOperation<consent::UserConsentVerificationResult> asking{nullptr};
        if (window != 0) {
            const auto interop = winrt::get_activation_factory<consent::UserConsentVerifier, IUserConsentVerifierInterop>();
            winrt::check_hresult(interop->RequestVerificationForWindowAsync(
                reinterpret_cast<HWND>(window), static_cast<HSTRING>(winrt::get_abi(text)),
                winrt::guid_of<decltype(asking)>(), winrt::put_abi(asking)));
        } else {
            asking = consent::UserConsentVerifier::RequestVerificationAsync(text);
        }
        switch (pending.wait(asking)) {
        case consent::UserConsentVerificationResult::Verified: return QuickUnlock::Result::Done;
        case consent::UserConsentVerificationResult::Canceled: return QuickUnlock::Result::Cancelled;
        case consent::UserConsentVerificationResult::RetriesExhausted: return QuickUnlock::Result::Failed;
        case consent::UserConsentVerificationResult::DeviceNotPresent:
        case consent::UserConsentVerificationResult::NotConfiguredForUser:
        case consent::UserConsentVerificationResult::DisabledByPolicy:
        case consent::UserConsentVerificationResult::DeviceBusy: return QuickUnlock::Result::Unavailable;
        }
        return QuickUnlock::Result::Failed;
    } catch (const winrt::hresult_error&) {
        return QuickUnlock::Result::Unavailable;
    }
}

// Windows' protection for this Windows account (DPAPI); empty when it fails.
QByteArray protect(const QByteArray& secret)
{
    DATA_BLOB in{static_cast<DWORD>(secret.size()), reinterpret_cast<BYTE*>(const_cast<char*>(secret.constData()))};
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"Chrono Vault", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return {};
    }
    QByteArray sealed(reinterpret_cast<const char*>(out.pbData), static_cast<qsizetype>(out.cbData));
    LocalFree(out.pbData);
    return sealed;
}

std::optional<QByteArray> unprotect(const QByteArray& sealed)
{
    DATA_BLOB in{static_cast<DWORD>(sealed.size()), reinterpret_cast<BYTE*>(const_cast<char*>(sealed.constData()))};
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return std::nullopt;
    }
    QByteArray secret(reinterpret_cast<const char*>(out.pbData), static_cast<qsizetype>(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return secret;
}

class WindowsQuickUnlock : public QuickUnlock
{
public:
    [[nodiscard]] bool available() const override
    {
        return std::async(std::launch::async, []() {
                   const HelloThread com;
                   try {
                       return consent::UserConsentVerifier::CheckAvailabilityAsync().get() ==
                              consent::UserConsentVerifierAvailability::Available;
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
        QPointer<WindowsQuickUnlock> self(this);
        // Its Cancel stops it too (D-042); a stopped one answers Cancelled.
        cancel();
        auto pending = std::make_shared<Pending>();
        asking = pending;
        const quintptr window = ownerWindow();
        const QString message = QCoreApplication::translate("QuickUnlock", "Turn on Windows Hello for Chrono Vault");
        logRequest("turning on", window);
        startBringingForward();
        emit fingerReady(); // Windows Hello shows its own window now
        std::thread([self, vault, secret, window, message, pending]() {
            const Result answer = verify(window, message, *pending);
            bool stopped = false;
            {
                const std::lock_guard<std::mutex> guard(pending->lock);
                stopped = pending->stopped;
            }
            QByteArray sealed;
            if (answer == Result::Done && !stopped) {
                sealed = protect(secret);
            }
            const Result result = stopped                ? Result::Cancelled
                                  : answer != Result::Done ? answer
                                  : sealed.isEmpty()     ? Result::NotKept
                                                         : Result::Done;
            QMetaObject::invokeMethod(
                qApp,
                [self, vault, sealed, result, answer, pending]() {
                    // Only the request still asking stops the listening: a
                    // cancelled one's late answer leaves the next one's alone.
                    if (self && self->asking == pending) {
                        stopBringingForward();
                    }
                    fingerLog(QStringLiteral("hello turning on: answered %1, kept: %2")
                                  .arg(static_cast<int>(answer))
                                  .arg(sealed.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes")));
                    if (!self) {
                        return;
                    }
                    if (!sealed.isEmpty()) {
                        self->keep(vault, sealed);
                    }
                    emit self->stored(result);
                },
                Qt::QueuedConnection);
        }).detach();
    }

    // After a master password change, while unlocked with it: kept again
    // without asking Windows Hello once more.
    void replace(const QString& vault, const QByteArray& secret) override
    {
        const QByteArray sealed = protect(secret);
        if (!sealed.isEmpty()) {
            keep(vault, sealed);
        }
        const Result result = sealed.isEmpty() ? Result::NotKept : Result::Done;
        QMetaObject::invokeMethod(this, [this, result]() { emit stored(result); }, Qt::QueuedConnection);
    }

    void retrieve(const QString& vault) override
    {
        const QByteArray sealed = QByteArray::fromBase64(VaultsFile().value(settingsKey(vault)).toString().toLatin1());
        if (sealed.isEmpty()) {
            // Nothing, or only a key kept the way before 0.4.72: turned on again.
            emit retrieved(Result::NoKey, {});
            return;
        }
        QPointer<WindowsQuickUnlock> self(this);
        cancel();
        auto pending = std::make_shared<Pending>();
        asking = pending;
        const quintptr window = ownerWindow();
        const QString message = QCoreApplication::translate("QuickUnlock", "Unlock Chrono Vault");
        logRequest("unlocking", window);
        startBringingForward();
        emit fingerReady(); // Windows Hello shows its own window now
        std::thread([self, sealed, window, message, pending]() {
            Result result = verify(window, message, *pending);
            {
                const std::lock_guard<std::mutex> guard(pending->lock);
                if (pending->stopped) {
                    return; // stopped: nobody waits for its answer
                }
            }
            QByteArray secret;
            if (result == Result::Done) {
                const std::optional<QByteArray> opened = unprotect(sealed);
                if (opened) {
                    secret = *opened;
                } else {
                    result = Result::NoKey; // another Windows account, or its protection reset
                }
            }
            QMetaObject::invokeMethod(
                qApp,
                [self, result, secret, pending]() {
                    if (self && self->asking == pending) {
                        stopBringingForward();
                    }
                    fingerLog(QStringLiteral("hello unlocking: answered %1").arg(static_cast<int>(result)));
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
        stopBringingForward();
        if (const auto pending = std::exchange(asking, nullptr)) {
            pending->stop();
        }
    }

    void remove(const QString& vault) override
    {
        VaultsFile().remove(settingsKey(vault));
        forgetOldKey(vault);
        remember(vault, false);
    }

private:
    void keep(const QString& vault, const QByteArray& sealed)
    {
        VaultsFile().setValue(settingsKey(vault), QString::fromLatin1(sealed.toBase64()));
        forgetOldKey(vault);
        remember(vault, true);
    }

    // A key kept the way before 0.4.72 goes, with its Windows Hello credential.
    static void forgetOldKey(const QString& vault)
    {
        VaultsFile settings;
        if (!settings.contains(oldSettingsKey(vault))) {
            return;
        }
        settings.remove(oldSettingsKey(vault));
        std::thread([vault]() {
            const HelloThread com;
            try {
                credentials::KeyCredentialManager::DeleteAsync(winrt::hstring(oldCredentialName(vault).toStdWString()))
                    .get();
            } catch (const winrt::hresult_error&) {
            }
        }).detach();
    }

    std::shared_ptr<Pending> asking; // the check now asking for the person

public:
    ~WindowsQuickUnlock() override { stopBringingForward(); }
};

} // namespace

std::unique_ptr<QuickUnlock> makeQuickUnlock()
{
    return std::make_unique<WindowsQuickUnlock>();
}

} // namespace chrono_vault
