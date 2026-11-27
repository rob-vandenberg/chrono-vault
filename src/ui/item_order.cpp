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
#include "item_order.h"

#include "settings_files.h"

#include <QCryptographicHash>
#include <QTimeZone>

#include <limits>

// --- Version ------------------------------------------------------------
// item_order.cpp 0.8.119

// --- Version history ----------------------------------------------------
// v0.8.119: Initial release (D-081)

namespace chrono_vault {

namespace {

// One group per vault in the vaults file, named after its place, as the PINs.
QString groupOf(const char* what, const QString& vaultFile)
{
    return QString::fromLatin1(what) + QLatin1Char('/') +
           QString::fromLatin1(QCryptographicHash::hash(vaultFile.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
}

// "<count> <milliseconds since 1970>"; anything else is no use at all.
ItemUse useFrom(const QString& kept)
{
    const QStringList parts = kept.split(QLatin1Char(' '));
    bool countRead = false;
    bool timeRead = false;
    const int count = parts.size() == 2 ? parts.at(0).toInt(&countRead) : 0;
    const qint64 when = parts.size() == 2 ? parts.at(1).toLongLong(&timeRead) : 0;
    if (!countRead || !timeRead || count < 1) {
        return {};
    }
    return {QDateTime::fromMSecsSinceEpoch(when, QTimeZone::UTC), count};
}

} // namespace

ItemOrder keptOrderOf(const QString& vaultFile)
{
    VaultsFile file;
    const QString group = groupOf("sorting", vaultFile);
    const int by = file.value(group + QStringLiteral("/by"), 0).toInt();
    ItemOrder order;
    order.by = by >= 0 && by < sortByCount ? static_cast<SortBy>(by) : SortBy::Title;
    order.descending = file.value(group + QStringLiteral("/descending"), false).toBool();
    return order;
}

void keepOrderOf(const QString& vaultFile, ItemOrder order)
{
    VaultsFile file;
    const QString group = groupOf("sorting", vaultFile);
    file.setValue(group + QStringLiteral("/by"), static_cast<int>(order.by));
    file.setValue(group + QStringLiteral("/descending"), order.descending);
}

QHash<QUuid, ItemUse> keptUsesOf(const QString& vaultFile)
{
    VaultsFile file;
    file.beginGroup(groupOf("uses", vaultFile));
    QHash<QUuid, ItemUse> uses;
    for (const QString& key : file.childKeys()) {
        const QUuid id = QUuid::fromString(key);
        const ItemUse use = useFrom(file.value(key).toString());
        if (!id.isNull() && use.count > 0) {
            uses.insert(id, use);
        }
    }
    return uses;
}

ItemUse keepUseOf(const QString& vaultFile, const QUuid& item, const QDateTime& when)
{
    VaultsFile file;
    const QString key = groupOf("uses", vaultFile) + QLatin1Char('/') + item.toString(QUuid::WithoutBraces);
    ItemUse use = useFrom(file.value(key).toString());
    use.count = use.count < std::numeric_limits<int>::max() ? use.count + 1 : use.count;
    use.last = when.toUTC();
    file.setValue(key, QStringLiteral("%1 %2").arg(use.count).arg(use.last.toMSecsSinceEpoch()));
    return use;
}

void forgetOrderOf(const QString& vaultFile)
{
    VaultsFile file;
    file.remove(groupOf("sorting", vaultFile));
    file.remove(groupOf("uses", vaultFile));
}

} // namespace chrono_vault
