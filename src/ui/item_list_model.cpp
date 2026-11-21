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
// item_list_model.cpp 0.0.4

// --- Version history ----------------------------------------------------
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
    case SubtitleRole: return fieldValue(item, QStringLiteral("username"));
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
            {IconRole, "icon"}};
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
