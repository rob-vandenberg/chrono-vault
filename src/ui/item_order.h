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
#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QUuid>

// --- Version ------------------------------------------------------------
// item_order.h 0.8.119

// --- Version history ----------------------------------------------------
// v0.8.119: Initial release: the item list's order per vault and each item's use, on this computer (D-081)

namespace chrono_vault {

// What the item list is sorted by (D-081), in the order of the Sort By menu.
enum class SortBy { Title, Url, Created, Modified, RecentlyUsed, FrequentlyUsed };
inline constexpr int sortByCount = 6;

struct ItemOrder
{
    SortBy by = SortBy::Title;
    bool descending = false;
};

// When an item was last used and how often: a field copied, its website
// opened, auto-type (D-081).
struct ItemUse
{
    QDateTime last;
    int count = 0;
};

// Kept on this computer only, never in the vault (the owner, D-081); a new
// vault, or one never sorted here, is Title, Ascending.
[[nodiscard]] ItemOrder keptOrderOf(const QString& vaultFile);
void keepOrderOf(const QString& vaultFile, ItemOrder order);
[[nodiscard]] QHash<QUuid, ItemUse> keptUsesOf(const QString& vaultFile);
// One more use of the item, now; gives the item's use as kept.
ItemUse keepUseOf(const QString& vaultFile, const QUuid& item, const QDateTime& when);
// A vault forgotten on this computer takes its order and uses with it.
void forgetOrderOf(const QString& vaultFile);

} // namespace chrono_vault
