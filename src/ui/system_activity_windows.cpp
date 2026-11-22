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
#include "system_activity.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QTimer>

#include <windows.h>

// --- Version ------------------------------------------------------------
// system_activity_windows.cpp 0.0.25

// --- Version history ----------------------------------------------------
// v0.0.25: Initial release (D-040)

// Windows: the power messages say when the computer goes to sleep and wakes;
// the time of the last input says how long the system has been inactive.

namespace chrono_vault {

namespace {

class WindowsSystemActivity : public SystemActivity, public QAbstractNativeEventFilter
{
public:
    WindowsSystemActivity()
    {
        if (QCoreApplication::instance()) {
            QCoreApplication::instance()->installNativeEventFilter(this);
        }
    }
    ~WindowsSystemActivity() override
    {
        if (QCoreApplication::instance()) {
            QCoreApplication::instance()->removeNativeEventFilter(this);
        }
    }
    WindowsSystemActivity(const WindowsSystemActivity&) = delete;
    WindowsSystemActivity& operator=(const WindowsSystemActivity&) = delete;

    void askIdleTime() override
    {
        LASTINPUTINFO last{};
        last.cbSize = sizeof(last);
        const qint64 seconds =
            GetLastInputInfo(&last) ? static_cast<qint64>((GetTickCount() - last.dwTime) / 1000) : -1;
        // Answered after this call returns, as every system does.
        QTimer::singleShot(0, this, [this, seconds]() { emit idleTime(seconds); });
    }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr*) override
    {
        if (eventType == "windows_generic_MSG") {
            const MSG* msg = static_cast<const MSG*>(message);
            if (msg->message == WM_POWERBROADCAST &&
                (msg->wParam == PBT_APMSUSPEND || msg->wParam == PBT_APMRESUMEAUTOMATIC)) {
                emit sleeping();
            }
        }
        return false;
    }
};

} // namespace

std::unique_ptr<SystemActivity> makeSystemActivity()
{
    return std::make_unique<WindowsSystemActivity>();
}

} // namespace chrono_vault
