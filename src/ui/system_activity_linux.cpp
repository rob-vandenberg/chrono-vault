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

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include <functional>

// --- Version ------------------------------------------------------------
// system_activity_linux.cpp 0.0.25

// --- Version history ----------------------------------------------------
// v0.0.25: Initial release (D-040)

// Linux: logind says when the computer goes to sleep and wakes; the desktop
// says how long the session has had no input (KDE and others through the
// freedesktop screen saver, GNOME through Mutter), both in milliseconds.

namespace chrono_vault {

namespace {

class LinuxSystemActivity : public SystemActivity
{
    Q_OBJECT

public:
    LinuxSystemActivity()
    {
        QDBusConnection::systemBus().connect(QStringLiteral("org.freedesktop.login1"),
                                             QStringLiteral("/org/freedesktop/login1"),
                                             QStringLiteral("org.freedesktop.login1.Manager"),
                                             QStringLiteral("PrepareForSleep"), this, SLOT(onPrepareForSleep(bool)));
    }

    void askIdleTime() override
    {
        ask(QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.ScreenSaver"),
                                           QStringLiteral("/org/freedesktop/ScreenSaver"),
                                           QStringLiteral("org.freedesktop.ScreenSaver"),
                                           QStringLiteral("GetSessionIdleTime")),
            [this]() {
                ask(QDBusMessage::createMethodCall(QStringLiteral("org.gnome.Mutter.IdleMonitor"),
                                                   QStringLiteral("/org/gnome/Mutter/IdleMonitor/Core"),
                                                   QStringLiteral("org.gnome.Mutter.IdleMonitor"),
                                                   QStringLiteral("GetIdletime")),
                    [this]() { emit idleTime(-1); });
            });
    }

private:
    // Asks without waiting; a number of milliseconds is the answer, anything
    // else tries the next way.
    void ask(const QDBusMessage& call, std::function<void()> otherwise)
    {
        auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, otherwise]() {
            watcher->deleteLater();
            const QDBusMessage reply = watcher->reply();
            bool ok = false;
            const qulonglong milliseconds = reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()
                                                ? reply.arguments().constFirst().toULongLong(&ok)
                                                : 0;
            if (ok) {
                emit idleTime(static_cast<qint64>(milliseconds / 1000));
            } else {
                otherwise();
            }
        });
    }

    // Going to sleep, or waking (in case going to sleep was missed).
    Q_SLOT void onPrepareForSleep(bool) { emit sleeping(); }
};

} // namespace

std::unique_ptr<SystemActivity> makeSystemActivity()
{
    return std::make_unique<LinuxSystemActivity>();
}

} // namespace chrono_vault

#include "system_activity_linux.moc"
