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

#include "item_differences.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <algorithm>

// --- Version ------------------------------------------------------------
// item_differences.cpp 0.5.78

// --- Version history ----------------------------------------------------
// v0.5.78: Initial release (D-061)

namespace chrono_vault {

namespace {

bool sameNote(const MergeNote& one, const MergeNote& other)
{
    return one.kind == other.kind && one.fieldUid == other.fieldUid && one.property == other.property &&
           one.subject == other.subject && one.otherValue == other.otherValue && one.otherData == other.otherData &&
           one.otherWhen == other.otherWhen;
}

// "notes.txt" kept as "notes (earlier).txt".
QString earlierName(const QString& name)
{
    const QString suffix = QFileInfo(name).completeSuffix();
    const QString base = suffix.isEmpty() ? name : name.left(name.size() - suffix.size() - 1);
    const QString earlier = QCoreApplication::translate("Attachment", "%1 (earlier)").arg(base);
    return suffix.isEmpty() ? earlier : earlier + QLatin1Char('.') + suffix;
}

void sortHistory(Field& field)
{
    std::sort(field.history.begin(), field.history.end(),
              [](const FieldChange& a, const FieldChange& b) { return a.changed < b.changed; });
}

} // namespace

QString flagText(bool on)
{
    return on ? QStringLiteral("1") : QStringLiteral("0");
}

bool addNote(QList<MergeNote>& notes, const MergeNote& note)
{
    if (std::any_of(notes.cbegin(), notes.cend(), [&note](const MergeNote& one) { return sameNote(one, note); })) {
        return false;
    }
    notes.append(note);
    return true;
}

NotedDifferences noteDifferences(Item& ours, const Item& theirs, const QDateTime& now, bool imported,
                                 const QString& device)
{
    NotedDifferences result;
    MergeNote blank;
    blank.when = now;
    blank.imported = imported;
    blank.device = device;
    const auto note = [&](qint64 fieldUid, const QString& property, const QString& other, const QString& subject = {},
                          const QByteArray& data = {}, const QDateTime& otherWhen = {}) {
        MergeNote one = blank;
        one.fieldUid = fieldUid;
        one.property = property;
        one.otherValue = other;
        one.subject = subject;
        one.otherData = data;
        one.otherWhen = otherWhen;
        if (addNote(ours.mergeNotes, one)) {
            result.changed = true;
            result.newConflict = true;
        }
    };
    const auto text = [&](QString Item::*member, const char* name) {
        if (ours.*member != theirs.*member) {
            note(0, QString::fromLatin1(name), theirs.*member);
        }
    };
    text(&Item::title, "title");
    text(&Item::subtitle, "subtitle");
    text(&Item::note, "note");
    text(&Item::website, "website");
    text(&Item::categoryId, "category");
    text(&Item::templateId, "template");
    const auto flag = [&](bool Item::*member, const char* name) {
        if (ours.*member != theirs.*member) {
            note(0, QString::fromLatin1(name), flagText(theirs.*member));
        }
    };
    flag(&Item::trashed, "trashed");
    flag(&Item::archived, "archived");
    flag(&Item::favourite, "favourite");
    flag(&Item::autoSubmit, "autoSubmit");
    if (ours.icon != theirs.icon) {
        note(0, QStringLiteral("icon"), QString(), QString(), theirs.icon);
    }
    for (const QString& tag : theirs.tags) {
        if (!ours.tags.contains(tag)) {
            note(0, QStringLiteral("tag"), tag);
        }
    }
    for (const Attachment& attachment : theirs.attachments) {
        const auto mine = std::find_if(ours.attachments.cbegin(), ours.attachments.cend(),
                                       [&attachment](const Attachment& one) { return one.id == attachment.id; });
        if (mine == ours.attachments.cend() || mine->name != attachment.name || mine->data != attachment.data) {
            note(0, QStringLiteral("attachment"), attachment.name, attachment.id.toString(QUuid::WithoutBraces),
                 attachment.data);
        }
    }
    for (const Field& field : theirs.fields) {
        if (field.uid == 0) {
            continue;
        }
        const auto mine = std::find_if(ours.fields.begin(), ours.fields.end(),
                                       [&field](const Field& one) { return one.uid == field.uid; });
        if (mine == ours.fields.end()) {
            if (field.deleted) {
                continue; // deleted there, never here: the same
            }
            // Only theirs has it: kept here as deleted until the person
            // chooses, so its name and kind are at hand (OR-15).
            Field kept = field;
            kept.deleted = true;
            kept.history.clear();
            ours.fields.append(kept);
            result.changed = true;
            note(field.uid, QStringLiteral("present"), field.value);
            continue;
        }
        if (mine->deleted != field.deleted) {
            if (mine->deleted) {
                note(field.uid, QStringLiteral("present"), field.value);
            } else {
                note(field.uid, QStringLiteral("absent"), QString());
            }
            continue;
        }
        if (field.deleted) {
            continue;
        }
        if (mine->value != field.value) {
            note(field.uid, QString(), field.value);
        }
        if (mine->label != field.label) {
            note(field.uid, QStringLiteral("label"), field.label);
        }
        if (mine->sensitive != field.sensitive) {
            note(field.uid, QStringLiteral("hidden"), flagText(field.sensitive));
        }
        for (const FieldChange& change : field.history) {
            if (!mine->history.contains(change)) {
                note(field.uid, QStringLiteral("history"), change.value, QString(), QByteArray(), change.changed);
            }
        }
    }
    return result;
}

void useOtherSide(Item& item, const MergeNote& note, const QDateTime& now)
{
    const QString& property = note.property;
    if (note.fieldUid != 0) {
        for (Field& field : item.fields) {
            if (field.uid != note.fieldUid) {
                continue;
            }
            if (property.isEmpty() || property == QLatin1String("present")) {
                if (field.value != note.otherValue) {
                    if (!field.value.isEmpty()) {
                        field.history.append(
                            FieldChange{field.value, field.changed.isValid() ? field.changed : now});
                    }
                    field.value = note.otherValue;
                    field.changed = now;
                }
                if (property == QLatin1String("present")) {
                    field.deleted = false;
                }
            } else if (property == QLatin1String("absent")) {
                field.deleted = true;
            } else if (property == QLatin1String("label")) {
                field.label = note.otherValue;
            } else if (property == QLatin1String("hidden")) {
                field.sensitive = note.otherValue == QLatin1String("1");
            } else if (property == QLatin1String("history")) {
                const FieldChange change{note.otherValue, note.otherWhen};
                if (!field.history.contains(change)) {
                    field.history.append(change);
                    sortHistory(field);
                }
            }
        }
        return;
    }
    const bool on = note.otherValue == QLatin1String("1");
    if (property == QLatin1String("title")) {
        item.title = note.otherValue;
    } else if (property == QLatin1String("subtitle")) {
        item.subtitle = note.otherValue;
    } else if (property == QLatin1String("note")) {
        item.note = note.otherValue;
    } else if (property == QLatin1String("website")) {
        item.website = note.otherValue;
    } else if (property == QLatin1String("category")) {
        item.categoryId = note.otherValue;
    } else if (property == QLatin1String("template")) {
        item.templateId = note.otherValue;
    } else if (property == QLatin1String("trashed")) {
        item.trashed = on;
    } else if (property == QLatin1String("archived")) {
        item.archived = on;
    } else if (property == QLatin1String("favourite")) {
        item.favourite = on;
    } else if (property == QLatin1String("autoSubmit")) {
        item.autoSubmit = on;
    } else if (property == QLatin1String("icon")) {
        item.icon = note.otherData;
    } else if (property == QLatin1String("tag")) {
        if (!item.tags.contains(note.otherValue)) {
            item.tags.append(note.otherValue);
        }
    } else if (property == QLatin1String("attachment")) {
        const QUuid id = QUuid::fromString(note.subject);
        const auto mine = std::find_if(item.attachments.begin(), item.attachments.end(),
                                       [&id](const Attachment& one) { return one.id == id; });
        if (mine == item.attachments.end()) {
            item.attachments.append(Attachment{id, note.otherValue, note.otherData});
        } else if (mine->name != note.otherValue || mine->data != note.otherData) {
            Attachment earlier = *mine;
            earlier.id = QUuid::createUuid();
            earlier.name = earlierName(earlier.name);
            mine->name = note.otherValue;
            mine->data = note.otherData;
            item.attachments.append(earlier);
        }
    } else if (property == QLatin1String("attachmentAbsent")) {
        const QUuid id = QUuid::fromString(note.subject);
        item.attachments.removeIf([&id](const Attachment& one) { return one.id == id; });
    }
}

void useOtherSide(VaultContents& contents, const MergeNote& note)
{
    if (note.property == QLatin1String("category") || note.property == QLatin1String("builtInCategory")) {
        QList<Category>& list =
            note.property == QLatin1String("category") ? contents.categories : contents.builtInCategoryChanges;
        const Category other{note.subject, note.otherValue, note.otherData};
        const auto mine =
            std::find_if(list.begin(), list.end(), [&note](const Category& one) { return one.id == note.subject; });
        if (mine == list.end()) {
            list.append(other);
        } else {
            *mine = other;
        }
    } else if (note.property == QLatin1String("template")) {
        const auto other = templateFromJson(note.otherData);
        if (!other) {
            return;
        }
        const auto mine = std::find_if(contents.templates.begin(), contents.templates.end(),
                                       [&note](const Template& one) { return one.id == note.subject; });
        if (mine == contents.templates.end()) {
            contents.templates.append(*other);
        } else {
            *mine = *other;
        }
    }
}

} // namespace chrono_vault
