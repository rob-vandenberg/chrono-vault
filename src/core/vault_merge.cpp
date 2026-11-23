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
#include "vault_merge.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <tuple>

// --- Version ------------------------------------------------------------
// vault_merge.cpp 0.2.45

// --- Version history ----------------------------------------------------
// v0.2.45: Initial release (D-054)

namespace chrono_vault {

namespace {

// One value of three: the side that changed it against the base; with no
// base, or when both changed it, the newer side, and that is a conflict.
template <class T>
T pick(const T* base, const T& mine, const T& theirs, bool theirsNewer, bool* conflict = nullptr)
{
    if (conflict != nullptr) {
        *conflict = false;
    }
    if (mine == theirs) {
        return mine;
    }
    if (base != nullptr) {
        if (mine == *base) {
            return theirs;
        }
        if (theirs == *base) {
            return mine;
        }
    }
    if (conflict != nullptr) {
        *conflict = true;
    }
    return theirsNewer ? theirs : mine;
}

// A set kept as a list: what either side added stays, what either side
// removed from the base goes; with no base, everything of both.
template <class T>
QList<T> mergeSet(const QList<T>* base, const QList<T>& mine, const QList<T>& theirs)
{
    QList<T> result;
    const auto keep = [&](const T& value) {
        if (result.contains(value)) {
            return;
        }
        const bool inMine = mine.contains(value);
        const bool inTheirs = theirs.contains(value);
        const bool inBase = base != nullptr && base->contains(value);
        if ((inMine && inTheirs) || ((inMine || inTheirs) && !inBase)) {
            result.append(value);
        }
    };
    for (const T& value : mine) {
        keep(value);
    }
    for (const T& value : theirs) {
        keep(value);
    }
    return result;
}

// Things with an id (categories, templates, attachments): each as pick()
// settles it; one on one side only is kept, unless the other side removed it
// from the base unchanged.
template <class T, class IdOf>
QList<T> mergeById(const QList<T>* base, const QList<T>& mine, const QList<T>& theirs, bool theirsNewer, IdOf idOf)
{
    const auto find = [&idOf](const QList<T>* list, const auto& id) -> const T* {
        if (list == nullptr) {
            return nullptr;
        }
        for (const T& one : *list) {
            if (idOf(one) == id) {
                return &one;
            }
        }
        return nullptr;
    };
    QList<T> result;
    for (const T& one : mine) {
        const T* other = find(&theirs, idOf(one));
        const T* before = find(base, idOf(one));
        if (other != nullptr) {
            result.append(pick(before, one, *other, theirsNewer));
        } else if (before == nullptr || !(*before == one)) {
            result.append(one); // new here, or changed here though removed there
        }
    }
    for (const T& one : theirs) {
        if (find(&mine, idOf(one)) != nullptr) {
            continue;
        }
        const T* before = find(base, idOf(one));
        if (before == nullptr || !(*before == one)) {
            result.append(one);
        }
    }
    return result;
}

// Everything of a field but its value, its time and its history.
auto shapeOf(const Field& field)
{
    return std::tie(field.role, field.label, field.kind, field.sensitive, field.deleted, field.hiddenType);
}

void addToHistory(QList<FieldChange>& history, const FieldChange& change)
{
    if (!change.value.isEmpty() && !history.contains(change)) {
        history.append(change);
    }
}

Field mergeField(const Field* base, const Field& mine, const Field& theirs, Item& item, const QDateTime& now,
                 bool& conflict)
{
    conflict = false;
    const bool theirsNewer = theirs.changed > mine.changed;
    Field result = mine;
    // The shape: as the side that changed it, else the newer.
    bool shapeConflict = false;
    const bool shapeFromTheirs = [&]() {
        if (shapeOf(mine) == shapeOf(theirs)) {
            return false;
        }
        if (base != nullptr && shapeOf(mine) == shapeOf(*base)) {
            return true;
        }
        if (base != nullptr && shapeOf(theirs) == shapeOf(*base)) {
            return false;
        }
        shapeConflict = true;
        return theirsNewer;
    }();
    if (shapeFromTheirs) {
        result = theirs;
    }
    (void)shapeConflict;
    // The history: both sides' earlier values, oldest first.
    QList<FieldChange> history = mine.history;
    for (const FieldChange& change : theirs.history) {
        addToHistory(history, change);
    }
    // The value.
    bool valueConflict = false;
    const QString value = pick(base != nullptr ? &base->value : nullptr, mine.value, theirs.value, theirsNewer,
                               &valueConflict);
    const bool valueFromTheirs = value != mine.value || (mine.value == theirs.value && theirsNewer);
    result.value = value;
    result.changed = valueFromTheirs ? theirs.changed : mine.changed;
    if (mine.value != theirs.value) {
        // The value that did not stand is an earlier value now.
        const Field& other = value == mine.value ? theirs : mine;
        addToHistory(history, FieldChange{other.value, other.changed.isValid() ? other.changed : now});
        if (valueConflict) {
            conflict = true;
            item.mergeNotes.append(MergeNote{MergeNote::Kind::ChangedOnBoth, mine.uid, QString(), other.value, now});
        }
        // A value changed on one side wins over the field deleted on the other.
        const Field& winner = value == mine.value ? mine : theirs;
        if (result.deleted && !winner.deleted) {
            result.deleted = false;
        }
    }
    std::sort(history.begin(), history.end(),
              [](const FieldChange& a, const FieldChange& b) { return a.changed < b.changed; });
    history.removeIf([&result](const FieldChange& change) {
        return change.value == result.value && change.changed == result.changed;
    });
    result.history = history;
    return result;
}

const Field* fieldWith(const QList<Field>* fields, qint64 uid)
{
    if (fields == nullptr || uid == 0) {
        return nullptr;
    }
    for (const Field& field : *fields) {
        if (field.uid == uid) {
            return &field;
        }
    }
    return nullptr;
}

QList<qint64> uidsOf(const QList<Field>& fields)
{
    QList<qint64> uids;
    for (const Field& field : fields) {
        uids.append(field.uid);
    }
    return uids;
}

QList<Field> mergeFields(const Item* base, const Item& mine, const Item& theirs, Item& result, const QDateTime& now,
                         bool& conflict)
{
    conflict = false;
    const QList<Field>* baseFields = base != nullptr ? &base->fields : nullptr;
    // The order: as the side that changed it, else this device's.
    const bool orderFromTheirs = baseFields != nullptr && uidsOf(mine.fields) == uidsOf(*baseFields);
    const QList<Field>& first = orderFromTheirs ? theirs.fields : mine.fields;
    const QList<Field>& second = orderFromTheirs ? mine.fields : theirs.fields;
    QList<qint64> order = uidsOf(first);
    qint64 previous = 0;
    for (const Field& field : second) {
        if (field.uid == 0 || order.contains(field.uid)) {
            previous = field.uid;
            continue;
        }
        const qsizetype at = previous == 0 ? 0 : order.indexOf(previous) + 1;
        order.insert(at, field.uid);
        previous = field.uid;
    }
    QList<Field> fields;
    for (const qint64 uid : std::as_const(order)) {
        if (uid == 0) {
            continue;
        }
        const Field* mineField = fieldWith(&mine.fields, uid);
        const Field* theirField = fieldWith(&theirs.fields, uid);
        const Field* baseField = fieldWith(baseFields, uid);
        if (mineField != nullptr && theirField != nullptr) {
            bool fieldConflict = false;
            fields.append(mergeField(baseField, *mineField, *theirField, result, now, fieldConflict));
            conflict = conflict || fieldConflict;
        } else {
            const Field* only = mineField != nullptr ? mineField : theirField;
            // Removed on the other side: gone, unless changed on this one.
            if (baseField == nullptr || !(*baseField == *only)) {
                fields.append(*only);
            }
        }
    }
    // Fields that never had an id: kept as they are, from both sides.
    for (const QList<Field>* side : {&mine.fields, &theirs.fields}) {
        for (const Field& field : *side) {
            if (field.uid == 0) {
                fields.append(field);
            }
        }
    }
    return fields;
}

Item mergeItem(const Item* base, const Item& mine, const Item& theirs, const QDateTime& now, bool& conflict)
{
    conflict = false;
    if (mine == theirs) {
        return mine;
    }
    if (base != nullptr && mine == *base) {
        return theirs;
    }
    if (base != nullptr && theirs == *base) {
        return mine;
    }
    const bool theirsNewer = theirs.modified > mine.modified;
    Item result = mine;
    result.mergeNotes =
        mergeSet<MergeNote>(base != nullptr ? &base->mergeNotes : nullptr, mine.mergeNotes, theirs.mergeNotes);
    // The item's own texts: a conflict leaves a note with the other value.
    const auto text = [&](QString Item::*member, const char* name) {
        bool textConflict = false;
        result.*member = pick(base != nullptr ? &(base->*member) : nullptr, mine.*member, theirs.*member, theirsNewer,
                              &textConflict);
        if (textConflict) {
            const QString& other = result.*member == mine.*member ? theirs.*member : mine.*member;
            result.mergeNotes.append(
                MergeNote{MergeNote::Kind::ChangedOnBoth, 0, QString::fromLatin1(name), other, now});
            conflict = true;
        }
    };
    text(&Item::title, "title");
    text(&Item::subtitle, "subtitle");
    text(&Item::note, "note");
    text(&Item::website, "website");
    // The rest: as the side that changed it, else the newer.
    const auto plain = [&](auto Item::*member) {
        result.*member = pick(base != nullptr ? &(base->*member) : nullptr, mine.*member, theirs.*member, theirsNewer);
    };
    plain(&Item::templateId);
    plain(&Item::categoryId);
    plain(&Item::favourite);
    plain(&Item::trashed);
    plain(&Item::archived);
    plain(&Item::autoSubmit);
    plain(&Item::icon);
    result.tags = mergeSet(base != nullptr ? &base->tags : nullptr, mine.tags, theirs.tags);
    bool fieldConflict = false;
    result.fields = mergeFields(base, mine, theirs, result, now, fieldConflict);
    conflict = conflict || fieldConflict;
    result.attachments = mergeById(base != nullptr ? &base->attachments : nullptr, mine.attachments,
                                   theirs.attachments, theirsNewer, [](const Attachment& one) { return one.id; });
    result.modified = std::max(mine.modified, theirs.modified);
    if (theirs.created.isValid() && (!mine.created.isValid() || theirs.created < mine.created)) {
        result.created = theirs.created;
    }
    return result;
}

const Item* itemWith(const QList<Item>* items, const QUuid& id)
{
    if (items == nullptr) {
        return nullptr;
    }
    for (const Item& item : *items) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}

const DeletedItem* deletionOf(const QList<DeletedItem>& deleted, const QUuid& id)
{
    for (const DeletedItem& one : deleted) {
        if (one.id == id) {
            return &one;
        }
    }
    return nullptr;
}

} // namespace

MergeResult mergeContents(const VaultContents* base, const VaultContents& mine, const VaultContents& theirs,
                          const QDateTime& now)
{
    MergeResult merged;
    VaultContents& result = merged.contents;
    const QList<Item>* baseItems = base != nullptr ? &base->items : nullptr;

    // An item on one side only: new there, or deleted on the other. Deleted
    // and changed after that: kept, with a note (the owner, Q8).
    const auto onOneSide = [&](const Item& item, const VaultContents& other) {
        const Item* before = itemWith(baseItems, item.id);
        const DeletedItem* deletion = deletionOf(other.deletedItems, item.id);
        bool deletedThere = false;
        bool changedSince = false;
        if (deletion != nullptr) {
            deletedThere = true;
            changedSince = item.modified > deletion->when;
        } else if (before != nullptr) {
            deletedThere = true; // gone there without a record: an older version deleted it
            changedSince = !(*before == item);
        }
        if (!deletedThere) {
            result.items.append(item);
            return;
        }
        if (!changedSince) {
            return; // deleted there, unchanged here: deleted
        }
        Item kept = item;
        const bool noted = std::any_of(kept.mergeNotes.cbegin(), kept.mergeNotes.cend(), [](const MergeNote& note) {
            return note.kind == MergeNote::Kind::DeletedElsewhere;
        });
        if (!noted) {
            kept.mergeNotes.append(MergeNote{MergeNote::Kind::DeletedElsewhere, 0, QString(), QString(), now});
        }
        ++merged.itemsBack;
        result.items.append(kept);
    };

    for (const Item& item : mine.items) {
        if (const Item* other = itemWith(&theirs.items, item.id)) {
            bool conflict = false;
            result.items.append(mergeItem(itemWith(baseItems, item.id), item, *other, now, conflict));
            if (conflict) {
                ++merged.itemsChangedOnBoth;
            }
        } else {
            onOneSide(item, theirs);
        }
    }
    for (const Item& item : theirs.items) {
        if (itemWith(&mine.items, item.id) == nullptr) {
            onOneSide(item, mine);
        }
    }

    // The deletions of both, except of items that were kept.
    for (const QList<DeletedItem>* side : {&mine.deletedItems, &theirs.deletedItems}) {
        for (const DeletedItem& one : *side) {
            if (itemWith(&result.items, one.id) != nullptr) {
                continue;
            }
            if (const DeletedItem* known = deletionOf(result.deletedItems, one.id)) {
                if (one.when < known->when) {
                    const_cast<DeletedItem*>(known)->when = one.when;
                }
                continue;
            }
            result.deletedItems.append(one);
        }
    }

    // The person's own categories and templates, hidden categories and icons.
    result.categories = mergeById(base != nullptr ? &base->categories : nullptr, mine.categories, theirs.categories,
                                  false, [](const Category& one) { return one.id; });
    result.templates = mergeById(base != nullptr ? &base->templates : nullptr, mine.templates, theirs.templates, false,
                                 [](const Template& one) { return one.id; });
    result.hiddenCategories = mergeSet(base != nullptr ? &base->hiddenCategories : nullptr, mine.hiddenCategories,
                                       theirs.hiddenCategories);
    result.customIcons =
        mergeSet(base != nullptr ? &base->customIcons : nullptr, mine.customIcons, theirs.customIcons);
    return merged;
}

} // namespace chrono_vault
