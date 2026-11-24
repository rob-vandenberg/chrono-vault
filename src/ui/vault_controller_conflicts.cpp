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


#include "vault_controller.h"

#include <QUuid>

#include <algorithm>

// --- Version ------------------------------------------------------------
// vault_controller_conflicts.cpp 0.5.76

// --- Version history ----------------------------------------------------
// v0.5.76: Initial release: the Sync conflicts dialog's rows, and settling them (D-060)

// The controller's part for the Sync conflicts dialog (D-060), after Chrono
// Mail's. A conflict is what a merge left on an item (D-054): the newest
// value stands, the other is kept with it until the person settles it, so
// nothing is lost meanwhile (OR-15).

namespace chrono_vault {

namespace {

// The name of what a note is about, and whether its value is shown as dots.
QString labelOf(const Item& item, const MergeNote& note, bool* hidden)
{
    *hidden = false;
    if (note.fieldUid != 0) {
        for (const Field& field : item.fields) {
            if (field.uid == note.fieldUid) {
                *hidden = field.isHidden();
                return field.isCustom() ? field.label : roleName(field.role);
            }
        }
        return QString();
    }
    if (note.property == QLatin1String("title")) {
        return VaultController::tr("Title");
    }
    if (note.property == QLatin1String("subtitle")) {
        return VaultController::tr("Subtitle");
    }
    if (note.property == QLatin1String("note")) {
        return VaultController::tr("Notes");
    }
    if (note.property == QLatin1String("website")) {
        return VaultController::tr("Website");
    }
    return QString();
}

// The value that stands now for what a note is about.
QString valueNow(const Item& item, const MergeNote& note)
{
    if (note.fieldUid != 0) {
        for (const Field& field : item.fields) {
            if (field.uid == note.fieldUid) {
                return field.value;
            }
        }
        return QString();
    }
    if (note.property == QLatin1String("title")) {
        return item.title;
    }
    if (note.property == QLatin1String("subtitle")) {
        return item.subtitle;
    }
    if (note.property == QLatin1String("note")) {
        return item.note;
    }
    if (note.property == QLatin1String("website")) {
        return item.website;
    }
    return QString();
}

// The other value put in place; the value it replaces goes into the field's
// history, as every change does.
void useOther(Item& item, const MergeNote& note, const QDateTime& now)
{
    if (note.fieldUid != 0) {
        for (Field& field : item.fields) {
            if (field.uid == note.fieldUid && field.value != note.otherValue) {
                if (!field.value.isEmpty()) {
                    field.history.append(FieldChange{field.value, field.changed.isValid() ? field.changed : now});
                }
                field.value = note.otherValue;
                field.changed = now;
            }
        }
    } else if (note.property == QLatin1String("title")) {
        item.title = note.otherValue;
    } else if (note.property == QLatin1String("subtitle")) {
        item.subtitle = note.otherValue;
    } else if (note.property == QLatin1String("note")) {
        item.note = note.otherValue;
    } else if (note.property == QLatin1String("website")) {
        item.website = note.otherValue;
    }
}

bool isDeletedElsewhere(const Item& item)
{
    return std::any_of(item.mergeNotes.cbegin(), item.mergeNotes.cend(),
                       [](const MergeNote& note) { return note.kind == MergeNote::Kind::DeletedElsewhere; });
}

} // namespace

int VaultController::conflictCount() const
{
    if (currentState != State::Unlocked) {
        return 0;
    }
    return static_cast<int>(std::count_if(contents.items.cbegin(), contents.items.cend(),
                                          [](const Item& item) { return !item.mergeNotes.isEmpty(); }));
}

QVariantList VaultController::conflicts() const
{
    QVariantList found;
    if (currentState != State::Unlocked) {
        return found;
    }
    for (const Item& item : contents.items) {
        if (item.mergeNotes.isEmpty()) {
            continue;
        }
        const QString id = item.id.toString(QUuid::WithoutBraces);
        QStringList labels;
        for (const MergeNote& note : item.mergeNotes) {
            bool hidden = false;
            const QString label = labelOf(item, note, &hidden);
            if (!label.isEmpty() && !labels.contains(label)) {
                labels.append(label);
            }
        }
        found.append(QVariantMap{
            {QStringLiteral("itemId"), id},
            {QStringLiteral("what"), this->item(id).value(QStringLiteral("templateName"))},
            {QStringLiteral("name"), item.title},
            {QStringLiteral("fields"), labels.isEmpty() ? QStringLiteral("—") : labels.join(QStringLiteral(", "))},
            {QStringLiteral("where"), vaultName()},
            {QStringLiteral("kind"), isDeletedElsewhere(item) ? QStringLiteral("deleted") : QStringLiteral("both")},
        });
    }
    return found;
}

QVariantMap VaultController::conflictOf(const QString& itemId) const
{
    if (currentState != State::Unlocked) {
        return {};
    }
    for (const Item& item : contents.items) {
        if (item.id.toString(QUuid::WithoutBraces) != itemId || item.mergeNotes.isEmpty()) {
            continue;
        }
        QVariantList rows;
        for (qsizetype index = 0; index < item.mergeNotes.size(); ++index) {
            const MergeNote& note = item.mergeNotes.at(index);
            if (note.kind != MergeNote::Kind::ChangedOnBoth) {
                continue;
            }
            bool hidden = false;
            const QString label = labelOf(item, note, &hidden);
            rows.append(QVariantMap{
                {QStringLiteral("index"), static_cast<int>(index)},
                {QStringLiteral("label"), label.isEmpty() ? tr("A field no longer in the item") : label},
                {QStringLiteral("now"), valueNow(item, note)},
                {QStringLiteral("other"), note.otherValue},
                {QStringLiteral("hidden"), hidden},
            });
        }
        return {
            {QStringLiteral("itemId"), itemId},
            {QStringLiteral("what"), this->item(itemId).value(QStringLiteral("templateName"))},
            {QStringLiteral("name"), item.title},
            {QStringLiteral("where"), vaultName()},
            {QStringLiteral("kind"), isDeletedElsewhere(item) ? QStringLiteral("deleted") : QStringLiteral("both")},
            {QStringLiteral("rows"), rows},
        };
    }
    return {};
}

void VaultController::settleConflicts(const QStringList& itemIds, const QString& side)
{
    if (currentState != State::Unlocked || itemIds.isEmpty()) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QList<Item> copies;
    bool changed = false;
    for (Item& item : contents.items) {
        if (item.mergeNotes.isEmpty() || !itemIds.contains(item.id.toString(QUuid::WithoutBraces))) {
            continue;
        }
        const QList<MergeNote> notes = std::exchange(item.mergeNotes, {});
        changed = true;
        if (side == QLatin1String("now")) {
            continue; // every value as it is; the others stay in the fields' history
        }
        const bool deletedElsewhere = std::any_of(notes.cbegin(), notes.cend(), [](const MergeNote& note) {
            return note.kind == MergeNote::Kind::DeletedElsewhere;
        });
        if (deletedElsewhere) {
            if (side == QLatin1String("other")) {
                // Deleted on the other side: into the Trash, never for good,
                // so it can still be restored.
                item.trashed = true;
                item.modified = now;
            }
            continue;
        }
        if (side == QLatin1String("both")) {
            // Kept as two: this one as it is, and a copy with the other values.
            Item copy = item;
            copy.id = QUuid::createUuid();
            copy.created = now;
            copy.modified = now;
            for (const MergeNote& note : notes) {
                useOther(copy, note, now);
            }
            copies.append(copy);
            continue;
        }
        for (const MergeNote& note : notes) {
            useOther(item, note, now);
        }
        item.modified = now;
    }
    contents.items.append(copies);
    if (changed) {
        contentsChanged();
        saveInBackground(QString());
    }
}

void VaultController::settleConflict(const QString& itemId, const QVariantList& choices)
{
    if (currentState != State::Unlocked) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (Item& item : contents.items) {
        if (item.id.toString(QUuid::WithoutBraces) != itemId || item.mergeNotes.isEmpty()) {
            continue;
        }
        QList<MergeNote> left;
        bool taken = false;
        for (qsizetype index = 0; index < item.mergeNotes.size(); ++index) {
            const MergeNote& note = item.mergeNotes.at(index);
            const QString choice = index < choices.size() ? choices.at(index).toString() : QString();
            if (note.kind != MergeNote::Kind::ChangedOnBoth || choice.isEmpty()) {
                left.append(note); // not chosen: it stays to be settled
                continue;
            }
            if (choice == QLatin1String("other")) {
                useOther(item, note, now);
                taken = true;
            }
        }
        item.mergeNotes = left;
        if (taken) {
            item.modified = now;
        }
        contentsChanged();
        saveInBackground(itemId);
        return;
    }
}

} // namespace chrono_vault
