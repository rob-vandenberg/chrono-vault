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
#include <limits>

// --- Version ------------------------------------------------------------
// item_list_model.cpp 0.8.119

// --- Version history ----------------------------------------------------
// v0.8.119: Sorted by title, web address, created, modified, recently or frequently used, either way; headings only
//           by title; headingRowOf (D-081)
// v0.6.87: A parameter no longer named as a member, for Microsoft's compiler
// v0.6.86: The lists of the audit (D-065)
// v0.5.82: rowStartingWith (D-062)
// v0.2.45: The mark of an item a merge left a note on (D-054)
// v0.1.41: idAt (D-051)
// v0.0.20: Website icons for items without an icon of their own (D-036)
// v0.0.17: The item's own subtitle first; Trash and Archive apart from the rest (D-033)
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

// The heading an item stands under by title: its first letter, or # for a
// title that starts with anything else (D-025).
QString initialOf(const Item& item)
{
    const QString title = item.title.trimmed();
    if (title.isEmpty() || !title.at(0).isLetter()) {
        return QStringLiteral("#");
    }
    return QString(title.at(0)).toUpper();
}

int compared(qint64 left, qint64 right)
{
    return left < right ? -1 : (left > right ? 1 : 0);
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
        // The item's own subtitle (D-033), else the first of these it has
        // (D-025), else its template's name.
        if (!item.subtitle.isEmpty()) {
            return item.subtitle;
        }
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
        // Under "reused": the items with one password together (D-065).
        if (chosen == QLatin1String("audit:reused")) {
            const int group = auditGroups.value(item.id);
            const auto count = std::count_if(shownItems.cbegin(), shownItems.cend(), [this, group](const Item& one) {
                return auditGroups.value(one.id) == group;
            });
            return tr("%1. The same password in %n item(s)", nullptr, static_cast<int>(count)).arg(group);
        }
        // Only by title (the owner, D-081): every other order is one list.
        return sorting.by == SortBy::Title ? initialOf(item) : QString();
    }
    case IsNoteRole: return item.categoryId == QLatin1String("note");
    case FavouriteRole: return item.favourite;
    case MergedRole: return !item.mergeNotes.isEmpty(); // something a merge left to settle (D-054)
    case IconRole:
        if (!item.icon.isEmpty()) {
            return QString(QStringLiteral("data:image/png;base64,") + QString::fromLatin1(item.icon.toBase64()));
        }
        if (const QString site = websiteIcons.value(websiteHostOf(item)); !site.isEmpty()) {
            return site;
        }
        return categoryIconSource(item.categoryId, allContents);
    default: return {};
    }
}

QHash<int, QByteArray> ItemListModel::roleNames() const
{
    return {{IdRole, "itemId"}, {TitleRole, "title"}, {SubtitleRole, "subtitle"}, {IsNoteRole, "isNote"},
            {IconRole, "icon"}, {InitialRole, "initial"}, {FavouriteRole, "favourite"}, {MergedRole, "merged"}};
}

void ItemListModel::setContents(const VaultContents& contents)
{
    allContents = contents;
    rebuild();
}

void ItemListModel::setWebsiteIcons(const QHash<QString, QString>& icons)
{
    websiteIcons = icons;
    if (!shownItems.isEmpty()) {
        emit dataChanged(index(0), index(static_cast<int>(shownItems.size()) - 1), {IconRole});
    }
}

void ItemListModel::setAudit(const QHash<QString, QSet<QUuid>>& lists, const QHash<QUuid, int>& groups)
{
    if (auditLists == lists && auditGroups == groups) {
        return;
    }
    auditLists = lists;
    auditGroups = groups;
    if (chosen.startsWith(QLatin1String("audit:"))) {
        rebuild();
    }
}

void ItemListModel::setFilter(const QString& filter)
{
    if (chosen != filter) {
        const bool before = headings();
        chosen = filter;
        rebuild();
        if (headings() != before) {
            emit headingsChanged();
        }
    }
}

void ItemListModel::setOrder(ItemOrder order, const QHash<QUuid, ItemUse>& uses)
{
    const bool before = headings();
    sorting = order;
    itemUses = uses;
    rebuild();
    if (headings() != before) {
        emit headingsChanged();
    }
}

void ItemListModel::setUse(const QUuid& item, const ItemUse& use)
{
    itemUses.insert(item, use);
    // The list moves only where the order is by use.
    if (sorting.by == SortBy::RecentlyUsed || sorting.by == SortBy::FrequentlyUsed) {
        rebuild();
    }
}

bool ItemListModel::headings() const
{
    return sorting.by == SortBy::Title || chosen == QLatin1String("audit:reused");
}

int ItemListModel::headingRowOf(const QString& letter) const
{
    if (sorting.by != SortBy::Title || letter.size() != 1 || chosen == QLatin1String("audit:reused")) {
        return -1;
    }
    const QChar typed = letter.at(0);
    const QString wanted = typed.isLetter() ? QString(typed).toUpper() : QStringLiteral("#");
    // A heading comes after the wanted one where it comes later in the
    // list's own direction; "#" stands first going up, last going down.
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    const auto rank = [&collator, &wanted](const QString& heading) {
        if (heading == wanted) {
            return 0;
        }
        if (wanted == QLatin1String("#")) {
            return 1;
        }
        if (heading == QLatin1String("#")) {
            return -1;
        }
        return collator.compare(heading, wanted);
    };
    for (int row = 0; row < shownItems.size(); ++row) {
        const int where = rank(initialOf(shownItems.at(row)));
        if (where == 0 || (sorting.descending ? where < 0 : where > 0)) {
            return row;
        }
    }
    return -1;
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
    auditLists.clear();
    auditGroups.clear();
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

QString ItemListModel::idAt(int row) const
{
    if (row < 0 || row >= shownItems.size()) {
        return {};
    }
    return shownItems.at(row).id.toString(QUuid::WithoutBraces);
}

int ItemListModel::rowStartingWith(const QString& text, int from, bool fromKept) const
{
    const int count = static_cast<int>(shownItems.size());
    const int start = std::max(from, -1) + (fromKept && from >= 0 ? 0 : 1);
    for (int step = 0; step < count && !text.isEmpty(); ++step) {
        const int row = (start + step) % count;
        if (shownItems.at(row).title.startsWith(text, Qt::CaseInsensitive)) {
            return row;
        }
    }
    return -1;
}

int ItemListModel::countOf(const QString& filter) const
{
    return static_cast<int>(std::count_if(allContents.items.cbegin(), allContents.items.cend(),
                                          [this, &filter](const Item& item) { return passesFilter(item, filter); }));
}

bool ItemListModel::passesFilter(const Item& item, const QString& filter) const
{
    // The Trash and the Archive show only their own items; every other
    // view leaves them out (D-033).
    if (filter == QLatin1String("trash")) {
        return item.trashed;
    }
    if (item.trashed) {
        return false;
    }
    if (filter == QLatin1String("archive")) {
        return item.archived;
    }
    if (item.archived) {
        return false;
    }
    if (filter.isEmpty()) {
        return true;
    }
    if (filter == QLatin1String("favourites")) {
        return item.favourite;
    }
    if (filter.startsWith(QLatin1String("audit:"))) {
        return auditLists.value(filter.mid(6)).contains(item.id);
    }
    if (filter.startsWith(QLatin1String("category:"))) {
        return item.categoryId == filter.mid(9);
    }
    if (filter.startsWith(QLatin1String("tag:"))) {
        const QString tag = filter.mid(4);
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
        if (!passesFilter(item, chosen)) {
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
    const auto lastUse = [this](const Item& item) {
        const ItemUse use = itemUses.value(item.id);
        return use.last.isValid() ? use.last.toMSecsSinceEpoch() : std::numeric_limits<qint64>::min();
    };
    // Ascending: A to Z, oldest first, least used first; Descending the
    // other way (the owner, D-081). Two alike stand by title, A to Z.
    const auto order = [&](const Item& left, const Item& right) {
        switch (sorting.by) {
        case SortBy::Title: return collator.compare(left.title, right.title);
        case SortBy::Url:
            return collator.compare(fieldValue(left, QStringLiteral("url")), fieldValue(right, QStringLiteral("url")));
        case SortBy::Created: return compared(left.created.toMSecsSinceEpoch(), right.created.toMSecsSinceEpoch());
        case SortBy::Modified: return compared(left.modified.toMSecsSinceEpoch(), right.modified.toMSecsSinceEpoch());
        case SortBy::RecentlyUsed: return compared(lastUse(left), lastUse(right));
        case SortBy::FrequentlyUsed: return compared(itemUses.value(left.id).count, itemUses.value(right.id).count);
        }
        return 0;
    };
    std::stable_sort(shownItems.begin(), shownItems.end(),
                     [this, &collator, &order](const Item& left, const Item& right) {
                         if (chosen == QLatin1String("audit:reused") && auditGroups.value(left.id) != auditGroups.value(right.id)) {
                             return auditGroups.value(left.id) < auditGroups.value(right.id);
                         }
                         const int first = order(left, right);
                         if (first != 0) {
                             return sorting.descending ? first > 0 : first < 0;
                         }
                         if (sorting.by == SortBy::Title) {
                             return false;
                         }
                         return collator.compare(left.title, right.title) < 0;
                     });
    endResetModel();
}

} // namespace chrono_vault
