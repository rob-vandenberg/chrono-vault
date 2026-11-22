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

#include "item_list_model.h"

#include <QCollator>

#include <algorithm>

// --- Version ------------------------------------------------------------
// item_list_model.cpp 0.0.9

// --- Version history ----------------------------------------------------
// v0.0.9: The letter an item is grouped under; the web address when there is no user name (D-025)
// v0.0.4: The item's icon, as an address QML can show
// v0.0.3: Initial release

namespace chrono_vault {

namespace {

QString fieldValue(const Item& item, const QString& role)
{
    for (const Field& field : item.fields) {
        if (field.role == role) {
            return field.value;
        }
    }
    return {};
}

} // namespace

ItemListModel::ItemListModel(QObject* parent) : QAbstractListModel(parent) {}

int ItemListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(shownItems.size());
}

QVariant ItemListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= shownItems.size()) {
        return {};
    }
    const Item& item = shownItems.at(index.row());
    switch (role) {
    case IdRole: return item.id.toString(QUuid::WithoutBraces);
    case TitleRole:
    case Qt::DisplayRole: return item.title;
    case SubtitleRole: {
        // The user name, or else the web address (D-025).
        const QString user = fieldValue(item, QStringLiteral("username"));
        return user.isEmpty() ? fieldValue(item, QStringLiteral("url")) : user;
    }
    case InitialRole: {
        // The heading the item stands under: its first letter, or # for a
        // title that starts with anything else (D-025).
        const QString title = item.title.trimmed();
        if (title.isEmpty() || !title.at(0).isLetter()) {
            return QStringLiteral("#");
        }
        return QString(title.at(0)).toUpper();
    }
    case IsNoteRole: return item.itemTemplate == ItemTemplate::SecureNote;
    case IconRole:
        if (!item.icon.isEmpty()) {
            return QString(QStringLiteral("data:image/png;base64,") + QString::fromLatin1(item.icon.toBase64()));
        }
        return item.itemTemplate == ItemTemplate::SecureNote ? QStringLiteral("qrc:/icons/item-note.svg")
                                                             : QStringLiteral("qrc:/icons/item-login.svg");
    default: return {};
    }
}

QHash<int, QByteArray> ItemListModel::roleNames() const
{
    return {{IdRole, "itemId"}, {TitleRole, "title"}, {SubtitleRole, "subtitle"}, {IsNoteRole, "isNote"},
            {IconRole, "icon"}, {InitialRole, "initial"}};
}

void ItemListModel::setItems(const QList<Item>& items)
{
    allItems = items;
    rebuild();
}

void ItemListModel::setSearchText(const QString& text)
{
    if (search != text) {
        search = text;
        rebuild();
    }
}

void ItemListModel::clear()
{
    allItems.clear();
    search.clear();
    rebuild();
}

int ItemListModel::rowOf(const QString& id) const
{
    for (int row = 0; row < shownItems.size(); ++row) {
        if (shownItems.at(row).id.toString(QUuid::WithoutBraces) == id) {
            return row;
        }
    }
    return -1;
}

void ItemListModel::rebuild()
{
    beginResetModel();
    shownItems.clear();
    const QString needle = search.trimmed();
    for (const Item& item : std::as_const(allItems)) {
        if (needle.isEmpty() || item.title.contains(needle, Qt::CaseInsensitive) ||
            fieldValue(item, QStringLiteral("username")).contains(needle, Qt::CaseInsensitive) ||
            fieldValue(item, QStringLiteral("url")).contains(needle, Qt::CaseInsensitive)) {
            shownItems.append(item);
        }
    }
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::stable_sort(shownItems.begin(), shownItems.end(),
                     [&collator](const Item& left, const Item& right) {
                         return collator.compare(left.title, right.title) < 0;
                     });
    endResetModel();
}

} // namespace chrono_vault
