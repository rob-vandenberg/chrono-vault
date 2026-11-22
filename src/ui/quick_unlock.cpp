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

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

// --- Version ------------------------------------------------------------
// quick_unlock.cpp 0.0.29

// --- Version history ----------------------------------------------------
// v0.0.29: The fingerprint log (D-044)
// v0.0.27: The words for a key not kept (D-042)
// v0.0.23: Initial release (D-038)

namespace chrono_vault {

namespace {

const QString keptKey = QStringLiteral("quickUnlock/vaults");

} // namespace

void fingerLog(const QString& step)
{
    const QString folder =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Chrono Vault");
    QDir().mkpath(folder);
    QFile file(folder + QStringLiteral("/fingerprint.log"));
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        file.write((QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz ")) + step +
                    QLatin1Char('\n'))
                       .toUtf8());
    }
}

QString QuickUnlock::notKeptText() const
{
    return QCoreApplication::translate("QuickUnlock", "The key could not be kept for this vault.");
}

bool QuickUnlock::has(const QString& vault) const
{
    return QSettings().value(keptKey).toStringList().contains(vault);
}

void QuickUnlock::remember(const QString& vault, bool kept)
{
    QSettings settings;
    QStringList vaults = settings.value(keptKey).toStringList();
    vaults.removeAll(vault);
    if (kept) {
        vaults.append(vault);
    }
    settings.setValue(keptKey, vaults);
}

} // namespace chrono_vault
