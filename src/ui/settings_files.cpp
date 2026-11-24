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

#include "settings_files.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

// --- Version ------------------------------------------------------------
// settings_files.cpp 0.5.78

// --- Version history ----------------------------------------------------
// v0.5.78: This device's own id (D-061)
// v0.0.38: Initial release (D-026)

namespace chrono_vault {

namespace {
QString vaultsFilePath()
{
    const QFileInfo settings(SettingsFile().fileName());
    return settings.absolutePath() + QLatin1Char('/') + settings.completeBaseName() + QStringLiteral(".conf");
}
} // namespace

SettingsFile::SettingsFile()
    : QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(),
                QCoreApplication::applicationName())
{
}

VaultsFile::VaultsFile() : QSettings(vaultsFilePath(), QSettings::IniFormat)
{
}

bool deleteSettingsFile()
{
    const QString file = SettingsFile().fileName();
    return !QFile::exists(file) || QFile::remove(file);
}

QString thisDeviceId()
{
    VaultsFile vaults;
    QString id = vaults.value(QStringLiteral("device/id")).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        vaults.setValue(QStringLiteral("device/id"), id);
    }
    return id;
}

} // namespace chrono_vault
