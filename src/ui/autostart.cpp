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

#include "autostart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

// --- Version ------------------------------------------------------------
// autostart.cpp 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Initial release

namespace chrono_vault::autostart {

namespace {

QString windowsKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString entryName = QStringLiteral("Chrono Vault");

QString linuxEntryPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
           QStringLiteral("/autostart/chrono-vault.desktop");
}

} // namespace

void setWindowsKeyForTests(const QString& key)
{
    windowsKey = key;
}

bool isEnabled()
{
#ifdef Q_OS_WIN
    return QSettings(windowsKey, QSettings::NativeFormat).contains(entryName);
#else
    return QFile::exists(linuxEntryPath());
#endif
}

bool setEnabled(bool enabled, const QString& program)
{
#ifdef Q_OS_WIN
    QSettings run(windowsKey, QSettings::NativeFormat);
    if (enabled) {
        run.setValue(entryName, QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(program)));
    } else {
        run.remove(entryName);
    }
    run.sync();
    return run.status() == QSettings::NoError && isEnabled() == enabled;
#else
    const QString path = linuxEntryPath();
    if (!enabled) {
        return !QFile::exists(path) || QFile::remove(path);
    }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QSaveFile entry(path);
    if (!entry.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    // Inside a quoted Exec value, " ` $ and \ are escaped with a backslash, and
    // % is written twice (the desktop entry specification).
    QString quoted;
    for (const QChar character : program) {
        if (character == QLatin1Char('"') || character == QLatin1Char('`') || character == QLatin1Char('$') ||
            character == QLatin1Char('\\')) {
            quoted += QLatin1Char('\\');
        } else if (character == QLatin1Char('%')) {
            quoted += QLatin1Char('%');
        }
        quoted += character;
    }
    const QString text = QStringLiteral("[Desktop Entry]\n"
                                        "Type=Application\n"
                                        "Name=Chrono Vault\n"
                                        "Exec=\"%1\"\n"
                                        "Icon=chrono-vault\n"
                                        "X-GNOME-Autostart-enabled=true\n")
                             .arg(quoted);
    entry.write(text.toUtf8());
    return entry.commit();
#endif
}

} // namespace chrono_vault::autostart
