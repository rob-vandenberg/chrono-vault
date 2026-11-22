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

#include "app_setup.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QSettings>
#include <QStandardPaths>
#include <QFontDatabase>
#include <QGuiApplication>
#include "window_memory.h"

#include <QQuickStyle>
#include <QtQml/QQmlExtensionPlugin>
#include <QWindow>

// --- Version ------------------------------------------------------------
// app_setup.cpp 0.0.10

// --- Version history ----------------------------------------------------
// v0.0.10: Chrono Vault's own style over Fusion, for the UI scaling (D-023)
// v0.0.4: Initial release

// Chrono Vault's style is part of the program itself (D-023).
Q_IMPORT_QML_PLUGIN(ChronoVault_StylePlugin)

namespace chrono_vault {

void applyStyle()
{
    // Fusion with its sizes following the UI scaling (D-017, D-023).
    QQuickStyle::setStyle(QStringLiteral("ChronoVault.Style"));
    for (const char* file : {":/fonts/NotoSans-Regular.ttf", ":/fonts/NotoSans-Bold.ttf",
                             ":/fonts/NotoSansMono-Regular.ttf"}) {
        QFontDatabase::addApplicationFont(QString::fromLatin1(file));
    }
    // The system's normal size, in Chrono Vault's own font.
    QFont font = QGuiApplication::font();
    font.setFamily(QStringLiteral("Noto Sans"));
    QGuiApplication::setFont(font);
}

void showMainWindow(QWindow* window, bool startMinimized, bool trayAvailable)
{
    if (!window) {
        return;
    }
    if (startMinimized && trayAvailable) {
        return; // it waits in the tray
    }
    if (startMinimized) {
        window->showMinimized();
        return;
    }
    // As it was left: maximized, minimized or normal (D-020).
    switch (WindowMemory::savedState(QStringLiteral("main"))) {
    case Qt::WindowMaximized: window->showMaximized(); break;
    case Qt::WindowMinimized: window->showMinimized(); break;
    case Qt::WindowFullScreen: window->showFullScreen(); break;
    default: window->show(); break;
    }
}

namespace {
bool& skippingSettings()
{
    static bool skip = false;
    return skip;
}
} // namespace

void setSkipReadingSettings(bool skip)
{
    skippingSettings() = skip;
}

bool skipReadingSettings()
{
    return skippingSettings();
}

bool carryOverOldSettings(const QString& oldOrganization, const QString& oldApplication)
{
    QSettings old(oldOrganization, oldApplication);
    QSettings now;
    const QStringList keys = old.allKeys();
    if (keys.isEmpty() || !now.allKeys().isEmpty()) {
        return true; // nothing to carry, or carried already
    }
    for (const QString& key : keys) {
        now.setValue(key, old.value(key));
    }
    now.sync();
    if (now.status() != QSettings::NoError) {
        return false;
    }
    for (const QString& key : keys) {
        if (now.value(key) != old.value(key)) {
            return false; // the old place stays: nothing is lost
        }
    }
    const QString oldFile = old.fileName();
    old.clear();
    old.sync();
    if (old.format() == QSettings::IniFormat || oldFile.endsWith(QLatin1String(".conf"))) {
        QFile::remove(oldFile);
        QDir().rmdir(QFileInfo(oldFile).absolutePath());
    }
    // The fingerprint log of 0.0.29 had a folder by the old name too.
    const QString oldLogFolder =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Chrono Vault");
    QFile::remove(oldLogFolder + QStringLiteral("/fingerprint.log"));
    QDir().rmdir(oldLogFolder);
    return true;
}

QIcon programIcon()
{
    return QIcon(QStringLiteral(":/icons/chrono-vault.svg"));
}

} // namespace chrono_vault
