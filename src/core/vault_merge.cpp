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

#include "item_differences.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <tuple>

// --- Version ------------------------------------------------------------
// vault_merge.cpp 0.5.81

// --- Version history ----------------------------------------------------
// v0.5.81: Which side deleted an item; an item brought back on one side is taken (D-061)
// v0.5.78: Every two-sided difference kept for the person to settle, this device's value standing (D-061)
// v0.2.56: The changes to built-in categories merged as the person's own categories (D-056)
// v0.2.45: Initial release (D-054)

namespace chrono_vault {

namespace {

// One value of three: the side that changed it against the base. With no
// base, or when both changed it, this device's value stands and that is a
// conflict for the person to settle (D-061).
template <class T>
T pick(const T* base, const T& mine, const T& theirs, bool* conflict = nullptr)
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
    return mine;
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

// Things with an id (the person's own categories and templates): each as
// pick() settles it, a conflict told to onConflict with the other side's;
// one on one side only is kept, unless the other side removed it from the
// base unchanged.
template <class T, class IdOf, class OnConflict>
QList<T> mergeById(const QList<T>* base, const QList<T>& mine, const QList<T>& theirs, IdOf idOf,
                   OnConflict onConflict)
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
            bool conflict = false;
            result.append(pick(before, one, *other, &conflict));
            if (conflict) {
                onConflict(*other);
            }
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

void addToHistory(QList<FieldChange>& history, const FieldChange& change)
{
    if (!change.value.isEmpty() && !history.contains(change)) {
        history.append(change);
    }
}

// Everything of a field but its time and its history.
auto contentOf(const Field& field)
{
    return std::tie(field.role, field.label, field.kind, field.sensitive, field.deleted, field.hiddenType, field.value);
}

// What a merge notes: a two-sided difference, this device's value standing.
MergeNote mergeNote(qint64 fieldUid, const QString& property, const QString& other, const QDateTime& now,
                    const QString& device)
{
    MergeNote note;
    note.fieldUid = fieldUid;
    note.property = property;
    note.otherValue = other;
    note.when = now;
    note.device = device;
    return note;
}

Field mergeField(const Field* base, const Field& mine, const Field& theirs, Item& item, const QDateTime& now,
                 const QString& device, bool& conflict)
{
    conflict = false;
    const auto noted = [&](const QString& property, const QString& other) {
        if (addNote(item.mergeNotes, mergeNote(mine.uid, property, other, now, device))) {
            conflict = true;
        }
    };
    // The history: both sides' earlier values, oldest first.
    QList<FieldChange> history = mine.history;
    for (const FieldChange& change : theirs.history) {
        addToHistory(history, change);
    }
    const auto finish = [&](Field result) {
        std::sort(history.begin(), history.end(),
                  [](const FieldChange& a, const FieldChange& b) { return a.changed < b.changed; });
        history.removeIf([&result](const FieldChange& change) {
            return change.value == result.value && change.changed == result.changed;
        });
        result.history = history;
        return result;
    };
    // Deleted on one side and not on the other: the side that changed it
    // against the base is taken; changed on both, this device's stands and
    // the person chooses (D-061).
    if (mine.deleted != theirs.deleted) {
        const bool mineChanged = base == nullptr || contentOf(mine) != contentOf(*base);
        const bool theirsChanged = base == nullptr || contentOf(theirs) != contentOf(*base);
        if (!mineChanged) {
            return finish(theirs);
        }
        if (theirsChanged) {
            if (mine.deleted) {
                noted(QStringLiteral("present"), theirs.value);
            } else {
                noted(QStringLiteral("absent"), QString());
            }
        }
        return finish(mine);
    }
    if (mine.deleted) {
        return finish(mine); // deleted on both
    }
    Field result = mine;
    bool labelConflict = false;
    result.label = pick(base != nullptr ? &base->label : nullptr, mine.label, theirs.label, &labelConflict);
    if (labelConflict) {
        noted(QStringLiteral("label"), theirs.label);
    }
    bool hiddenConflict = false;
    result.sensitive =
        pick(base != nullptr ? &base->sensitive : nullptr, mine.sensitive, theirs.sensitive, &hiddenConflict);
    if (hiddenConflict) {
        noted(QStringLiteral("hidden"), flagText(theirs.sensitive));
    }
    // What the person cannot change in a window: as the side that changed it.
    result.role = pick(base != nullptr ? &base->role : nullptr, mine.role, theirs.role);
    result.kind = pick(base != nullptr ? &base->kind : nullptr, mine.kind, theirs.kind);
    result.hiddenType = pick(base != nullptr ? &base->hiddenType : nullptr, mine.hiddenType, theirs.hiddenType);
    // The value.
    bool valueConflict = false;
    result.value =
        pick(base != nullptr ? &base->value : nullptr, mine.value, theirs.value, &valueConflict);
    const bool valueFromTheirs = result.value != mine.value;
    result.changed = valueFromTheirs ? theirs.changed : mine.changed;
    if (mine.value != theirs.value) {
        // The value that did not stand is an earlier value now.
        const Field& other = valueFromTheirs ? mine : theirs;
        addToHistory(history, FieldChange{other.value, other.changed.isValid() ? other.changed : now});
        if (valueConflict) {
            noted(QString(), theirs.value);
        }
    }
    return finish(result);
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
                         const QString& device, bool& conflict)
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
            fields.append(mergeField(baseField, *mineField, *theirField, result, now, device, fieldConflict));
            conflict = conflict || fieldConflict;
        } else {
            const Field* only = mineField != nullptr ? mineField : theirField;
            if (baseField == nullptr) {
                fields.append(*only); // new on one side
                continue;
            }
            if (*baseField == *only) {
                continue; // removed on the other side, unchanged on this one
            }
            // Changed on one side and removed on the other: the person
            // chooses; this device's stands meanwhile (D-061).
            if (only == mineField) {
                fields.append(*only);
                if (addNote(result.mergeNotes, mergeNote(only->uid, QStringLiteral("absent"), QString(), now, device))) {
                    conflict = true;
                }
            } else {
                Field kept = *only;
                kept.deleted = true;
                fields.append(kept);
                if (addNote(result.mergeNotes,
                            mergeNote(only->uid, QStringLiteral("present"), only->value, now, device))) {
                    conflict = true;
                }
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

// Attachments by id: as the side that changed them; changed on both, or
// changed on one and removed on the other, this device's stands and the
// person chooses (D-061).
QList<Attachment> mergeAttachments(const Item* base, const Item& mine, const Item& theirs, Item& result,
                                   const QDateTime& now, const QString& device, bool& conflict)
{
    const auto find = [](const QList<Attachment>* list, const QUuid& id) -> const Attachment* {
        if (list == nullptr) {
            return nullptr;
        }
        for (const Attachment& one : *list) {
            if (one.id == id) {
                return &one;
            }
        }
        return nullptr;
    };
    const QList<Attachment>* baseList = base != nullptr ? &base->attachments : nullptr;
    const auto noted = [&](const QString& property, const Attachment& about, const Attachment* other) {
        MergeNote note = mergeNote(0, property, other != nullptr ? other->name : QString(), now, device);
        note.subject = about.id.toString(QUuid::WithoutBraces);
        if (other != nullptr) {
            note.otherData = other->data;
        }
        if (addNote(result.mergeNotes, note)) {
            conflict = true;
        }
    };
    QList<Attachment> merged;
    for (const Attachment& one : mine.attachments) {
        const Attachment* other = find(&theirs.attachments, one.id);
        const Attachment* before = find(baseList, one.id);
        if (other != nullptr) {
            bool both = false;
            merged.append(pick(before, one, *other, &both));
            if (both) {
                noted(QStringLiteral("attachment"), one, other);
            }
        } else if (before == nullptr) {
            merged.append(one); // new here
        } else if (!(*before == one)) {
            merged.append(one); // changed here, removed there
            noted(QStringLiteral("attachmentAbsent"), one, nullptr);
        }
    }
    for (const Attachment& one : theirs.attachments) {
        if (find(&mine.attachments, one.id) != nullptr) {
            continue;
        }
        const Attachment* before = find(baseList, one.id);
        if (before == nullptr) {
            merged.append(one); // new there
        } else if (!(*before == one)) {
            noted(QStringLiteral("attachment"), one, &one); // changed there, removed here
        }
    }
    return merged;
}

Item mergeItem(const Item* base, const Item& mine, const Item& theirs, const QDateTime& now, const QString& device,
               bool& conflict)
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
    Item result = mine;
    result.mergeNotes =
        mergeSet<MergeNote>(base != nullptr ? &base->mergeNotes : nullptr, mine.mergeNotes, theirs.mergeNotes);
    result.modified = std::max(mine.modified, theirs.modified);
    if (theirs.created.isValid() && (!mine.created.isValid() || theirs.created < mine.created)) {
        result.created = theirs.created;
    }
    if (base == nullptr) {
        // Nothing to judge by: every difference waits for the person, this
        // device's values standing (D-061).
        conflict = noteDifferences(result, theirs, now, false, device).newConflict;
        return result;
    }
    const auto noted = [&](const MergeNote& note) {
        if (addNote(result.mergeNotes, note)) {
            conflict = true;
        }
    };
    // The item's own texts and the rest: as the side that changed it; changed
    // on both, this device's stands and the other waits in a note.
    const auto text = [&](QString Item::*member, const char* name) {
        bool both = false;
        result.*member = pick(&(base->*member), mine.*member, theirs.*member, &both);
        if (both) {
            noted(mergeNote(0, QString::fromLatin1(name), theirs.*member, now, device));
        }
    };
    text(&Item::title, "title");
    text(&Item::subtitle, "subtitle");
    text(&Item::note, "note");
    text(&Item::website, "website");
    text(&Item::templateId, "template");
    text(&Item::categoryId, "category");
    const auto flag = [&](bool Item::*member, const char* name) {
        bool both = false;
        result.*member = pick(&(base->*member), mine.*member, theirs.*member, &both);
        if (both) {
            noted(mergeNote(0, QString::fromLatin1(name), flagText(theirs.*member), now, device));
        }
    };
    flag(&Item::favourite, "favourite");
    flag(&Item::trashed, "trashed");
    flag(&Item::archived, "archived");
    flag(&Item::autoSubmit, "autoSubmit");
    flag(&Item::noAudit, "noAudit");
    bool iconConflict = false;
    result.icon = pick(&base->icon, mine.icon, theirs.icon, &iconConflict);
    if (iconConflict) {
        MergeNote note = mergeNote(0, QStringLiteral("icon"), QString(), now, device);
        note.otherData = theirs.icon;
        noted(note);
    }
    result.tags = mergeSet(&base->tags, mine.tags, theirs.tags);
    bool fieldConflict = false;
    result.fields = mergeFields(base, mine, theirs, result, now, device, fieldConflict);
    bool attachmentConflict = false;
    result.attachments = mergeAttachments(base, mine, theirs, result, now, device, attachmentConflict);
    conflict = conflict || fieldConflict || attachmentConflict;
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
                          const QDateTime& now, const QString& device)
{
    MergeResult merged;
    VaultContents& result = merged.contents;
    const QList<Item>* baseItems = base != nullptr ? &base->items : nullptr;

    // An item on one side only: new there, or deleted on the other. Deleted
    // and changed after that: kept, with a note (the owner, Q8).
    const auto onOneSide = [&](const Item& item, const VaultContents& other, bool ours) {
        const Item* before = itemWith(baseItems, item.id);
        if (base != nullptr && before == nullptr && deletionOf(base->deletedItems, item.id) != nullptr) {
            // Deleted in what both agreed on, and brought back since on this
            // one side (kept by the person, or imported): taken (D-061).
            result.items.append(item);
            return;
        }
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
            MergeNote note;
            note.kind = MergeNote::Kind::DeletedElsewhere;
            note.when = now;
            // Which side deleted it, for the windows to say (D-061).
            note.device = device;
            if (!ours) {
                note.property = QStringLiteral("deletedOn");
            }
            kept.mergeNotes.append(note);
        }
        ++merged.itemsBack;
        result.items.append(kept);
    };

    for (const Item& item : mine.items) {
        if (const Item* other = itemWith(&theirs.items, item.id)) {
            bool conflict = false;
            result.items.append(mergeItem(itemWith(baseItems, item.id), item, *other, now, device, conflict));
            if (conflict) {
                ++merged.itemsChangedOnBoth;
            }
        } else {
            onOneSide(item, theirs, true);
        }
    }
    for (const Item& item : theirs.items) {
        if (itemWith(&mine.items, item.id) == nullptr) {
            onOneSide(item, mine, false);
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
    // Changed differently on both: this device's stands, the other waits in
    // a note of the vault for the person (D-061).
    result.vaultNotes =
        mergeSet(base != nullptr ? &base->vaultNotes : nullptr, mine.vaultNotes, theirs.vaultNotes);
    const auto vaultNote = [&](const QString& property, const QString& subject, const QString& other,
                               const QByteArray& data) {
        MergeNote note = mergeNote(0, property, other, now, device);
        note.subject = subject;
        note.otherData = data;
        addNote(result.vaultNotes, note);
    };
    result.categories = mergeById(base != nullptr ? &base->categories : nullptr, mine.categories, theirs.categories,
                                  [](const Category& one) { return one.id; }, [&](const Category& other) {
                                      vaultNote(QStringLiteral("category"), other.id, other.name, other.icon);
                                  });
    result.builtInCategoryChanges = mergeById(
        base != nullptr ? &base->builtInCategoryChanges : nullptr, mine.builtInCategoryChanges,
        theirs.builtInCategoryChanges, [](const Category& one) { return one.id; },
        [&](const Category& other) { vaultNote(QStringLiteral("builtInCategory"), other.id, other.name, other.icon); });
    result.templates = mergeById(base != nullptr ? &base->templates : nullptr, mine.templates, theirs.templates,
                                 [](const Template& one) { return one.id; }, [&](const Template& other) {
                                     vaultNote(QStringLiteral("template"), other.id, QString(), templateToJson(other));
                                 });
    result.hiddenCategories = mergeSet(base != nullptr ? &base->hiddenCategories : nullptr, mine.hiddenCategories,
                                       theirs.hiddenCategories);
    result.customIcons =
        mergeSet(base != nullptr ? &base->customIcons : nullptr, mine.customIcons, theirs.customIcons);
    return merged;
}

} // namespace chrono_vault
