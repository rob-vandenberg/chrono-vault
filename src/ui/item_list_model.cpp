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

#include "templates.h"

#include <QCollator>

#include <algorithm>

// --- Version ------------------------------------------------------------
// item_list_model.cpp 0.0.14

// --- Version history ----------------------------------------------------
// v0.0.14: Narrowed to a category, a tag or the favourites; the subtitle and icon of every template; the search also
//          finds an email address and a tag (D-027, D-028)
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
        // The first of these the item has (D-025), else its template's name.
        for (const char* fieldRole : {"username", "email", "url", "cardholder", "fullname", "network", "bankname",
                                      "product", "organisation"}) {
            const QString value = fieldValue(item, QString::fromLatin1(fieldRole));
            if (!value.isEmpty()) {
                return value;
            }
        }
        const auto itemTemplate = findTemplate(item.templateId, allContents);
        return itemTemplate ? templateName(*itemTemplate) : categoryName(item.categoryId, allContents);
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
    case IsNoteRole: return item.categoryId == QLatin1String("note");
    case FavouriteRole: return item.favourite;
    case IconRole:
        if (!item.icon.isEmpty()) {
            return QString(QStringLiteral("data:image/png;base64,") + QString::fromLatin1(item.icon.toBase64()));
        }
        return categoryIconSource(item.categoryId);
    default: return {};
    }
}

QHash<int, QByteArray> ItemListModel::roleNames() const
{
    return {{IdRole, "itemId"}, {TitleRole, "title"}, {SubtitleRole, "subtitle"}, {IsNoteRole, "isNote"},
            {IconRole, "icon"}, {InitialRole, "initial"}, {FavouriteRole, "favourite"}};
}

void ItemListModel::setContents(const VaultContents& contents)
{
    allContents = contents;
    rebuild();
}

void ItemListModel::setFilter(const QString& filter)
{
    if (chosen != filter) {
        chosen = filter;
        rebuild();
    }
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
    allContents = {};
    search.clear();
    chosen.clear();
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

bool ItemListModel::passesFilter(const Item& item) const
{
    if (chosen.isEmpty()) {
        return true;
    }
    if (chosen == QLatin1String("favourites")) {
        return item.favourite;
    }
    if (chosen.startsWith(QLatin1String("category:"))) {
        return item.categoryId == chosen.mid(9);
    }
    if (chosen.startsWith(QLatin1String("tag:"))) {
        const QString tag = chosen.mid(4);
        return std::any_of(item.tags.cbegin(), item.tags.cend(), [&tag](const QString& one) {
            return one == tag || one.startsWith(tag + QLatin1Char('/'));
        });
    }
    return true;
}

void ItemListModel::rebuild()
{
    beginResetModel();
    shownItems.clear();
    const QString needle = search.trimmed();
    for (const Item& item : std::as_const(allContents.items)) {
        if (!passesFilter(item)) {
            continue;
        }
        const auto found = [&needle](const QString& text) { return text.contains(needle, Qt::CaseInsensitive); };
        if (needle.isEmpty() || found(item.title) || found(fieldValue(item, QStringLiteral("username"))) ||
            found(fieldValue(item, QStringLiteral("url"))) || found(fieldValue(item, QStringLiteral("email"))) ||
            std::any_of(item.tags.cbegin(), item.tags.cend(), found)) {
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
