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

#include "vault_contents.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

// --- Version ------------------------------------------------------------
// item_list_model.h 0.5.82

// --- Version history ----------------------------------------------------
// v0.5.82: rowStartingWith (D-062)
// v0.2.45: The mark of an item a merge left a note on (D-054)
// v0.1.41: idAt (D-051)
// v0.0.20: Website icons for items without an icon of their own (D-036)
// v0.0.14: Narrowed to a category, a tag or the favourites; the subtitle and icon of every template (D-027, D-028)
// v0.0.9: The letter an item is grouped under (D-025)
// v0.0.4: The item's icon
// v0.0.3: Initial release

namespace chrono_vault {

// The items of the unlocked vault as the list in the middle shows them:
// sorted by title, narrowed by what is chosen on the left (everything, the
// favourites, a category or a tag) and by the search text.
class ItemListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The controller owns the list")

public:
    enum Role { IdRole = Qt::UserRole + 1, TitleRole, SubtitleRole, IsNoteRole, IconRole, InitialRole, FavouriteRole, MergedRole };

    explicit ItemListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setContents(const VaultContents& contents);
    void setSearchText(const QString& text);
    // "" for every item, "favourites", "category:<id>" or "tag:<tag>"; a
    // tag also shows the items of the tags nested under it (D-028).
    void setFilter(const QString& filter);
    // Website icons by host, as "data:" addresses ready to show, for an item
    // without its own (D-036); only the icons are redrawn, the list stays.
    void setWebsiteIcons(const QHash<QString, QString>& icons);
    void clear();

    // The row of the item with this id, or -1.
    [[nodiscard]] Q_INVOKABLE int rowOf(const QString& id) const;
    // The id of the item in this row, or nothing.
    [[nodiscard]] Q_INVOKABLE QString idAt(int row) const;
    // The next row after this one whose title starts with the text, going
    // round; -1 when none does. With from kept, the row itself counts first.
    [[nodiscard]] Q_INVOKABLE int rowStartingWith(const QString& text, int from, bool fromKept) const;
    // How many items a view of the left column holds ("" for Home,
    // "favourites", "category:<id>", "tag:<tag>", "archive", "trash"),
    // whatever the search (D-039).
    [[nodiscard]] Q_INVOKABLE int countOf(const QString& filter) const;

private:
    void rebuild();

    [[nodiscard]] static bool passesFilter(const Item& item, const QString& filter);

    VaultContents allContents;
    QList<Item> shownItems;
    QString search;
    QString chosen;
    QHash<QString, QString> websiteIcons;
};

} // namespace chrono_vault
