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

#include "item_differences.h"
#include "settings_files.h"
#include "templates.h"

#include <QLocale>
#include <QUuid>

#include <algorithm>
#include <functional>

// --- Version ------------------------------------------------------------
// vault_controller_conflicts.cpp 0.5.81

// --- Version history ----------------------------------------------------
// v0.5.81: Every field of the item; which side deleted it; a split keeps both values; Keep here only, and what Retry sync can read (D-061)
// v0.5.78: Every kind of difference as a row; the sides "Vault" and "Import", or "This device" and "Other device"; the person's own categories and templates (D-061)
// v0.5.77: A conflict from an import: kind "imported" (D-060)
// v0.5.76: Initial release: the Sync conflicts dialog's rows, and settling them (D-060)

// The controller's part for the Sync conflicts dialog (D-060, D-061), after
// Chrono Mail's. A conflict is what a merge or an import left: one value
// stands, the other is kept with it until the person chooses, so nothing is
// lost meanwhile (OR-15). The program never chooses: it cannot know which
// value is right.
//
// Each conflict has two sides, shown left and right: for an import the vault
// and the import, for a sync this device and the other device. A note keeps
// the device whose value stands; on any other device the two sides swap.

namespace chrono_vault {

namespace {

const QString vaultPrefix = QStringLiteral("vault:");

QString nameOfField(const Field& field)
{
    return field.isCustom() ? field.label : roleName(field.role);
}

const Field* fieldOf(const Item& item, qint64 uid)
{
    for (const Field& field : item.fields) {
        if (field.uid == uid) {
            return &field;
        }
    }
    return nullptr;
}

const Attachment* attachmentOf(const Item& item, const QString& id)
{
    const QUuid uuid = QUuid::fromString(id);
    for (const Attachment& one : item.attachments) {
        if (one.id == uuid) {
            return &one;
        }
    }
    return nullptr;
}

// The other side's value stands on this device: the note was made on
// another device, whose value stands (D-061).
bool swappedHere(const MergeNote& note, const QString& device)
{
    return !note.imported && !note.device.isEmpty() && note.device != device;
}

// The side's word ("left" or "right", or "now" and "other" as before)
// turned into whether the note's other value is taken.
bool takesOther(const MergeNote& note, const QString& side, const QString& device)
{
    if (side == QLatin1String("other")) {
        return true;
    }
    if (side == QLatin1String("now")) {
        return false;
    }
    const bool right = side == QLatin1String("right");
    return swappedHere(note, device) ? !right : right;
}

// The item was deleted on one side and changed on the other: "there" when
// the other device deleted it, "here" when this one did, "" when neither. A
// note of before 0.5.81 does not say which; it was kept as changed here.
QString deletedOn(const QList<MergeNote>& notes, const QString& device)
{
    for (const MergeNote& note : notes) {
        if (note.kind != MergeNote::Kind::DeletedElsewhere) {
            continue;
        }
        const bool mine = note.device.isEmpty() || note.device == device;
        const bool byThatDevice = note.property == QLatin1String("deletedOn");
        return mine == byThatDevice && !note.device.isEmpty() ? QStringLiteral("here") : QStringLiteral("there");
    }
    return QString();
}

// "imported": every note from an import; "both": none; "mixed": some.
QString sourceOf(const QList<MergeNote>& notes)
{
    const auto imported = std::count_if(notes.cbegin(), notes.cend(), [](const MergeNote& one) { return one.imported; });
    return imported == notes.size() ? QStringLiteral("imported")
           : imported == 0          ? QStringLiteral("both")
                                    : QStringLiteral("mixed");
}

QString kindOf(const Item& item, const QString& device)
{
    const QString deleted = deletedOn(item.mergeNotes, device);
    if (!deleted.isEmpty()) {
        return deleted == QLatin1String("here") ? QStringLiteral("deletedHere") : QStringLiteral("deletedThere");
    }
    return sourceOf(item.mergeNotes) == QLatin1String("imported") ? QStringLiteral("imported")
                                                                  : QStringLiteral("both");
}

// Retry sync can read this item again: from the shared vault, or from where
// every imported value came from.
bool canRetry(const QList<MergeNote>& notes)
{
    return std::all_of(notes.cbegin(), notes.cend(),
                       [](const MergeNote& one) { return !one.imported || !one.source.isEmpty(); });
}

// When a conflict was found: its oldest note.
QString foundText(const QList<MergeNote>& notes)
{
    QDateTime oldest;
    for (const MergeNote& note : notes) {
        if (note.when.isValid() && (!oldest.isValid() || note.when < oldest)) {
            oldest = note.when;
        }
    }
    return oldest.isValid() ? oldest.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QString();
}

// A vault note's group in the list: "vault:<property>:<subject>".
QString groupOf(const MergeNote& note)
{
    return vaultPrefix + note.property + QLatin1Char(':') + note.subject;
}

} // namespace

// The row a note makes: its name, the value standing and the other value, as
// the person reads them, and whether they are shown as dots.
QVariantMap VaultController::conflictRow(const Item& item, const MergeNote& note) const
{
    const QString notOn = tr("(not on the item)");
    const auto yesNo = [this](bool on) { return on ? tr("Yes") : tr("No"); };
    const QString& property = note.property;
    const QString deleted = tr("(deleted)");
    QString label;
    QString standing;
    QString other;
    bool hidden = false;
    if (note.fieldUid != 0) {
        const Field* field = fieldOf(item, note.fieldUid);
        const QString name = field != nullptr ? nameOfField(*field) : tr("A field no longer in the item");
        hidden = field != nullptr && field->isHidden();
        const QString value = field != nullptr ? field->value : QString();
        label = name;
        if (property.isEmpty()) {
            standing = value;
            other = note.otherValue;
        } else if (property == QLatin1String("present")) {
            standing = note.subject == QLatin1String("never") ? notOn : deleted;
            other = note.otherValue;
        } else if (property == QLatin1String("absent")) {
            standing = value;
            other = deleted;
        } else if (property == QLatin1String("label")) {
            label = tr("Name of “%1”").arg(name);
            standing = field != nullptr ? field->label : QString();
            other = note.otherValue;
            hidden = false;
        } else if (property == QLatin1String("hidden")) {
            label = tr("“%1” shown as dots").arg(name);
            standing = yesNo(field != nullptr && field->sensitive);
            other = yesNo(note.otherValue == QLatin1String("1"));
            hidden = false;
        } else if (property == QLatin1String("history")) {
            label = tr("Earlier value of “%1”").arg(name);
            standing = tr("(not in its history)");
            other = tr("%1 (%2)").arg(note.otherValue,
                                       QLocale().toString(note.otherWhen.toLocalTime(), QLocale::ShortFormat));
        }
    } else if (property == QLatin1String("title") || property == QLatin1String("subtitle") ||
               property == QLatin1String("note") || property == QLatin1String("website")) {
        label = property == QLatin1String("title")      ? tr("Title")
                : property == QLatin1String("subtitle") ? tr("Subtitle")
                : property == QLatin1String("note")     ? tr("Notes")
                                                        : tr("Website");
        standing = property == QLatin1String("title")      ? item.title
                   : property == QLatin1String("subtitle") ? item.subtitle
                   : property == QLatin1String("note")     ? item.note
                                                           : item.website;
        other = note.otherValue;
    } else if (property == QLatin1String("category")) {
        label = tr("Category");
        standing = categoryName(item.categoryId, contents);
        other = categoryName(note.otherValue, contents);
    } else if (property == QLatin1String("template")) {
        label = tr("Template");
        const auto mine = findTemplate(item.templateId, contents);
        const auto theirs = findTemplate(note.otherValue, contents);
        standing = mine ? templateName(*mine) : item.templateId;
        other = theirs ? templateName(*theirs) : note.otherValue;
    } else if (property == QLatin1String("trashed") || property == QLatin1String("archived") ||
               property == QLatin1String("favourite") || property == QLatin1String("autoSubmit") ||
               property == QLatin1String("noAudit")) {
        label = property == QLatin1String("trashed")     ? tr("In the Trash")
                : property == QLatin1String("archived")  ? tr("In the Archive")
                : property == QLatin1String("favourite") ? tr("Favorite")
                : property == QLatin1String("noAudit")   ? tr("Excluded from Audit")
                                                         : tr("Autosubmit Login");
        const bool on = property == QLatin1String("trashed")     ? item.trashed
                        : property == QLatin1String("archived")  ? item.archived
                        : property == QLatin1String("favourite") ? item.favourite
                        : property == QLatin1String("noAudit")   ? item.noAudit
                                                                 : item.autoSubmit;
        standing = yesNo(on);
        other = yesNo(note.otherValue == QLatin1String("1"));
    } else if (property == QLatin1String("icon")) {
        label = tr("Icon");
        standing = item.icon.isEmpty() ? tr("The category's icon") : tr("An icon of its own");
        other = note.otherData.isEmpty() ? tr("The category's icon")
                : item.icon.isEmpty()    ? tr("An icon of its own")
                                         : tr("Another icon of its own");
    } else if (property == QLatin1String("tag")) {
        label = tr("Tag");
        standing = item.tags.contains(note.otherValue) ? note.otherValue : notOn;
        other = note.otherValue;
    } else if (property == QLatin1String("attachment") || property == QLatin1String("attachmentAbsent")) {
        label = tr("Attachment");
        const Attachment* mine = attachmentOf(item, note.subject);
        standing = mine != nullptr ? tr("%1 (%2)").arg(mine->name, sizeText(mine->data.size())) : notOn;
        other = property == QLatin1String("attachmentAbsent")
                    ? notOn
                    : tr("%1 (%2)").arg(note.otherValue, sizeText(note.otherData.size()));
    } else if (property == QLatin1String("deletedHere")) {
        label = tr("The item");
        standing = tr("Deleted for good");
        other = tr("Kept");
    }
    // A word about a value is never shown as dots.
    const auto said = [&](QString& value) {
        if (value.isEmpty()) {
            value = tr("(empty)");
            return true;
        }
        return property == QLatin1String("present") || property == QLatin1String("absent")
                   ? value == notOn || value == deleted
                   : false;
    };
    const bool history = property == QLatin1String("history") && note.fieldUid != 0;
    const bool standingSaid = said(standing) || history;
    const bool otherSaid = said(other);
    return {{QStringLiteral("label"), label},
            {QStringLiteral("standing"), standing},
            {QStringLiteral("other"), other},
            {QStringLiteral("hidden"), hidden},
            {QStringLiteral("standingHidden"), hidden && !standingSaid},
            {QStringLiteral("otherHidden"), hidden && !otherSaid}};
}

// Every field of an item in its order, a conflict's rows where their field
// stands (D-061): the same on both sides plain, a difference to be chosen. An
// empty field the same on both sides is left out, as the item itself shows it.
QVariantList VaultController::conflictFields(const Item& item, const QVariantList& choices,
                                             const QString& deletedSide) const
{
    QVariantList shown;
    QList<int> taken;
    const auto differing = [&](const std::function<bool(const MergeNote&)>& about) {
        bool value = false;
        for (const QVariant& choice : choices) {
            const QVariantMap row = choice.toMap();
            const int index = row.value(QStringLiteral("index")).toInt();
            const MergeNote& note = item.mergeNotes.at(index);
            if (taken.contains(index) || !about(note)) {
                continue;
            }
            taken.append(index);
            shown.append(row);
            value = value || note.fieldUid == 0 || note.property.isEmpty() ||
                    note.property == QLatin1String("present") || note.property == QLatin1String("absent");
        }
        return value;
    };
    const auto same = [&](const QString& label, const QString& value, bool hidden) {
        if (value.isEmpty()) {
            return;
        }
        const QString gone = tr("(deleted)");
        const bool leftGone = deletedSide == QLatin1String("here");
        const bool rightGone = deletedSide == QLatin1String("there");
        shown.append(QVariantMap{
            {QStringLiteral("index"), -1},
            {QStringLiteral("label"), label},
            {QStringLiteral("left"), leftGone ? gone : value},
            {QStringLiteral("right"), rightGone ? gone : value},
            {QStringLiteral("leftHidden"), hidden && !leftGone},
            {QStringLiteral("rightHidden"), hidden && !rightGone},
            {QStringLiteral("differs"), false},
            {QStringLiteral("canSplit"), false},
        });
    };
    const auto property = [&](const char* name, const QString& label, const QString& value) {
        if (!differing([name](const MergeNote& note) {
                return note.fieldUid == 0 && note.property == QLatin1String(name);
            })) {
            same(label, value, false);
        }
    };
    property("title", tr("Title"), item.title);
    property("subtitle", tr("Subtitle"), item.subtitle);
    for (const Field& field : item.fields) {
        const bool itsValue = field.uid != 0 && differing([&field](const MergeNote& note) {
            return note.fieldUid == field.uid;
        });
        if (itsValue || field.deleted || field.kind == FieldKind::Section || field.kind == FieldKind::Hidden) {
            continue;
        }
        same(nameOfField(field), field.value, field.isHidden());
    }
    property("note", tr("Notes"), item.note);
    property("website", tr("Website"), item.website);
    property("category", tr("Category"), categoryName(item.categoryId, contents));
    (void)differing([](const MergeNote& note) { return note.property == QLatin1String("template"); });
    for (const QString& tag : item.tags) {
        same(tr("Tag"), tag, false);
    }
    for (const Attachment& one : item.attachments) {
        const QString id = one.id.toString(QUuid::WithoutBraces);
        if (!differing([&id](const MergeNote& note) {
                return note.fieldUid == 0 && note.subject == id && note.property.startsWith(QLatin1String("attachment"));
            })) {
            same(tr("Attachment"), tr("%1 (%2)").arg(one.name, sizeText(one.data.size())), false);
        }
    }
    (void)differing([](const MergeNote&) { return true; }); // whatever is left
    return shown;
}

// What a window offers for a conflict, and when it was found.
QVariantMap VaultController::conflictWays(const QString& kind, const QList<MergeNote>& notes, bool ofVault) const
{
    const bool deleted = kind.startsWith(QLatin1String("deleted"));
    const bool imported = std::any_of(notes.cbegin(), notes.cend(), [](const MergeNote& one) { return one.imported; });
    return {
        {QStringLiteral("canRetry"), canRetry(notes) && !(ofVault && imported)},
        {QStringLiteral("canKeepHere"), kind == QLatin1String("deletedThere")},
        {QStringLiteral("canKeepBoth"), !deleted && !ofVault},
        {QStringLiteral("found"), foundText(notes)},
    };
}

// The two sides' names (D-061): an import's "Vault" and "Import", a sync's
// "This device" and "Other device"; both when an item holds both kinds.
QVariantMap VaultController::sideNames(const QString& source) const
{
    if (source == QLatin1String("imported")) {
        return {{QStringLiteral("left"), tr("Vault")}, {QStringLiteral("right"), tr("Import")}};
    }
    if (source == QLatin1String("both")) {
        return {{QStringLiteral("left"), tr("This device")}, {QStringLiteral("right"), tr("Other device")}};
    }
    return {{QStringLiteral("left"), tr("Vault / This device")}, {QStringLiteral("right"), tr("Import / Other device")}};
}

int VaultController::conflictCount() const
{
    if (currentState != State::Unlocked) {
        return 0;
    }
    QStringList groups;
    for (const MergeNote& note : contents.vaultNotes) {
        if (!groups.contains(groupOf(note))) {
            groups.append(groupOf(note));
        }
    }
    return static_cast<int>(std::count_if(contents.items.cbegin(), contents.items.cend(),
                                          [](const Item& item) { return !item.mergeNotes.isEmpty(); })) +
           static_cast<int>(groups.size());
}

QVariantList VaultController::conflicts() const
{
    QVariantList found;
    if (currentState != State::Unlocked) {
        return found;
    }
    const QString device = thisDeviceId();
    for (const Item& item : contents.items) {
        if (item.mergeNotes.isEmpty()) {
            continue;
        }
        const QString id = item.id.toString(QUuid::WithoutBraces);
        QStringList labels;
        for (const MergeNote& note : item.mergeNotes) {
            const QString label =
                note.kind == MergeNote::Kind::DeletedElsewhere ? QString() : conflictRow(item, note).value(QStringLiteral("label")).toString();
            if (!label.isEmpty() && !labels.contains(label)) {
                labels.append(label);
            }
        }
        const QString kind = kindOf(item, device);
        QVariantMap one{
            {QStringLiteral("itemId"), id},
            {QStringLiteral("what"), this->item(id).value(QStringLiteral("templateName"))},
            {QStringLiteral("name"), item.title},
            {QStringLiteral("fields"), labels.isEmpty() ? QStringLiteral("—") : labels.join(QStringLiteral(", "))},
            {QStringLiteral("where"), vaultName()},
            {QStringLiteral("kind"), kind},
            {QStringLiteral("source"), sourceOf(item.mergeNotes)},
            {QStringLiteral("ofVault"), false},
        };
        one.insert(conflictWays(kind, item.mergeNotes, false));
        found.append(one);
    }
    // The person's own categories and templates that differ.
    QStringList groups;
    for (const MergeNote& note : contents.vaultNotes) {
        const QString group = groupOf(note);
        if (groups.contains(group)) {
            continue;
        }
        groups.append(group);
        const QVariantMap shown = conflictOf(group);
        found.append(QVariantMap{
            {QStringLiteral("itemId"), group},
            {QStringLiteral("what"), shown.value(QStringLiteral("what"))},
            {QStringLiteral("name"), shown.value(QStringLiteral("name"))},
            {QStringLiteral("fields"), shown.value(QStringLiteral("fields"))},
            {QStringLiteral("where"), vaultName()},
            {QStringLiteral("kind"), shown.value(QStringLiteral("kind"))},
            {QStringLiteral("source"), shown.value(QStringLiteral("source"))},
            {QStringLiteral("ofVault"), true},
            {QStringLiteral("canRetry"), shown.value(QStringLiteral("canRetry"))},
            {QStringLiteral("canKeepHere"), false},
            {QStringLiteral("canKeepBoth"), false},
        });
    }
    return found;
}

QVariantMap VaultController::conflictOf(const QString& itemId) const
{
    if (currentState != State::Unlocked) {
        return {};
    }
    const QString device = thisDeviceId();
    const auto rowFor = [&device](int index, const QVariantMap& shown, const MergeNote& note) {
        const bool swapped = swappedHere(note, device);
        return QVariantMap{
            {QStringLiteral("index"), index},
            {QStringLiteral("label"), shown.value(QStringLiteral("label"))},
            {QStringLiteral("left"), shown.value(swapped ? QStringLiteral("other") : QStringLiteral("standing"))},
            {QStringLiteral("right"), shown.value(swapped ? QStringLiteral("standing") : QStringLiteral("other"))},
            {QStringLiteral("hidden"), shown.value(QStringLiteral("hidden"))},
            {QStringLiteral("leftHidden"),
             shown.value(swapped ? QStringLiteral("otherHidden") : QStringLiteral("standingHidden"))},
            {QStringLiteral("rightHidden"),
             shown.value(swapped ? QStringLiteral("standingHidden") : QStringLiteral("otherHidden"))},
            {QStringLiteral("differs"), true},
            {QStringLiteral("canSplit"), false},
            // Both values, in the order a split keeps them.
            {QStringLiteral("first"), shown.value(QStringLiteral("standing"))},
            {QStringLiteral("second"), shown.value(QStringLiteral("other"))},
        };
    };
    if (itemId.startsWith(vaultPrefix)) {
        QVariantList rows;
        QList<MergeNote> notes;
        QString what;
        QString name;
        for (qsizetype index = 0; index < contents.vaultNotes.size(); ++index) {
            const MergeNote& note = contents.vaultNotes.at(index);
            if (groupOf(note) != itemId) {
                continue;
            }
            notes.append(note);
            QString standing;
            QString other;
            if (note.property == QLatin1String("template")) {
                what = tr("Template");
                const auto mine = findTemplate(note.subject, contents);
                const auto theirs = templateFromJson(note.otherData);
                name = mine ? templateName(*mine) : note.subject;
                const auto fieldNames = [](const Template& one) {
                    QStringList names;
                    for (const Field& field : one.fields) {
                        if (!field.deleted) {
                            names.append(nameOfField(field));
                        }
                    }
                    return names.join(QStringLiteral(", "));
                };
                standing = mine ? fieldNames(*mine) : QString();
                other = theirs ? fieldNames(*theirs) : QString();
                rows.append(rowFor(static_cast<int>(index),
                                   {{QStringLiteral("label"), tr("Fields")},
                                    {QStringLiteral("standing"), standing},
                                    {QStringLiteral("other"), other},
                                    {QStringLiteral("hidden"), false}},
                                   note));
            } else {
                what = tr("Category");
                name = categoryName(note.subject, contents);
                const QList<Category>& list = note.property == QLatin1String("category")
                                                  ? contents.categories
                                                  : contents.builtInCategoryChanges;
                const auto mine = std::find_if(list.cbegin(), list.cend(),
                                               [&note](const Category& one) { return one.id == note.subject; });
                standing = name;
                other = note.otherValue.isEmpty() ? categoryName(note.subject, VaultContents()) : note.otherValue;
                if (mine != list.cend() && mine->icon != note.otherData) {
                    other = tr("%1 (another icon)").arg(other);
                }
                rows.append(rowFor(static_cast<int>(index),
                                   {{QStringLiteral("label"), tr("Category")},
                                    {QStringLiteral("standing"), standing},
                                    {QStringLiteral("other"), other},
                                    {QStringLiteral("hidden"), false}},
                                   note));
            }
        }
        if (rows.isEmpty()) {
            return {};
        }
        const QString source = sourceOf(notes);
        QVariantMap names = sideNames(source);
        QVariantMap whole{
            {QStringLiteral("itemId"), itemId},
            {QStringLiteral("what"), what},
            {QStringLiteral("name"), name},
            {QStringLiteral("fields"), what == tr("Template") ? tr("Fields") : tr("Category")},
            {QStringLiteral("where"), vaultName()},
            {QStringLiteral("kind"), source == QLatin1String("imported") ? source : QStringLiteral("both")},
            {QStringLiteral("source"), source},
            {QStringLiteral("ofVault"), true},
            {QStringLiteral("leftName"), names.value(QStringLiteral("left"))},
            {QStringLiteral("rightName"), names.value(QStringLiteral("right"))},
            {QStringLiteral("rows"), rows},
            {QStringLiteral("allRows"), rows},
        };
        whole.insert(conflictWays(QStringLiteral("both"), notes, true));
        return whole;
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
            QVariantMap row = rowFor(static_cast<int>(index), conflictRow(item, note), note);
            row.insert(QStringLiteral("canSplit"), canKeepBoth(item, note));
            rows.append(row);
        }
        const QString deletedSide = deletedOn(item.mergeNotes, device);
        if (!deletedSide.isEmpty()) {
            rows.clear(); // a whole side is chosen
        }
        const QString kind = kindOf(item, device);
        const QString source = sourceOf(item.mergeNotes);
        const QVariantMap names = sideNames(source);
        QVariantMap whole{
            {QStringLiteral("itemId"), itemId},
            {QStringLiteral("what"), this->item(itemId).value(QStringLiteral("templateName"))},
            {QStringLiteral("name"), item.title},
            {QStringLiteral("where"), vaultName()},
            {QStringLiteral("kind"), kind},
            {QStringLiteral("source"), source},
            {QStringLiteral("ofVault"), false},
            {QStringLiteral("leftName"), names.value(QStringLiteral("left"))},
            {QStringLiteral("rightName"), names.value(QStringLiteral("right"))},
            {QStringLiteral("rows"), rows},
            {QStringLiteral("allRows"), conflictFields(item, rows, deletedSide)},
        };
        whole.insert(conflictWays(kind, item.mergeNotes, false));
        return whole;
    }
    return {};
}

// One note settled on its item: the other side taken, or the standing one
// kept. An item deleted for good here whose deletion stands is deleted for
// good again; true then, so the caller removes it.
bool VaultController::settleNote(Item& item, const MergeNote& note, bool takeOther, const QDateTime& now)
{
    if (note.property == QLatin1String("deletedHere")) {
        return !takeOther;
    }
    if (takeOther) {
        useOtherSide(item, note, now);
        item.modified = now;
    }
    return false;
}

void VaultController::settleConflicts(const QStringList& itemIds, const QString& side)
{
    if (currentState != State::Unlocked || itemIds.isEmpty()) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QString device = thisDeviceId();
    QList<Item> copies;
    QList<QUuid> deleteForGood;
    bool changed = false;
    // The person's own categories and templates.
    QList<MergeNote> keptVaultNotes;
    for (const MergeNote& note : std::as_const(contents.vaultNotes)) {
        if (!itemIds.contains(groupOf(note)) || side == QLatin1String("both") || side == QLatin1String("keep")) {
            keptVaultNotes.append(note);
            continue;
        }
        changed = true;
        if (takesOther(note, side, device)) {
            useOtherSide(contents, note);
        }
    }
    contents.vaultNotes = keptVaultNotes;
    for (Item& item : contents.items) {
        if (item.mergeNotes.isEmpty() || !itemIds.contains(item.id.toString(QUuid::WithoutBraces))) {
            continue;
        }
        const QString deletedSide = deletedOn(item.mergeNotes, device);
        if (!deletedSide.isEmpty()) {
            if (side == QLatin1String("both") ||
                (side == QLatin1String("keep") && deletedSide != QLatin1String("there"))) {
                continue; // not a way to settle this one
            }
            // The deletion taken: into the Trash, never for good, so it can
            // still be restored. Kept: counted as changed now, so the next
            // sync puts it back on the other device.
            const bool deletion = side == QLatin1String("other") ||
                                  side == (deletedSide == QLatin1String("there") ? QLatin1String("right")
                                                                                 : QLatin1String("left"));
            item.mergeNotes.clear();
            item.trashed = item.trashed || deletion;
            item.modified = now;
            changed = true;
            continue;
        }
        if (side == QLatin1String("keep")) {
            continue;
        }
        const QList<MergeNote> notes = std::exchange(item.mergeNotes, {});
        changed = true;
        if (side == QLatin1String("both")) {
            // Kept as two: this one as it is, and a copy with the other values.
            Item copy = item;
            copy.id = QUuid::createUuid();
            copy.created = now;
            copy.modified = now;
            for (const MergeNote& note : notes) {
                useOtherSide(copy, note, now);
            }
            copies.append(copy);
            continue;
        }
        for (const MergeNote& note : notes) {
            if (settleNote(item, note, takesOther(note, side, device), now)) {
                deleteForGood.append(item.id);
            }
        }
    }
    contents.items.append(copies);
    removeForGood(deleteForGood, now);
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
    const QString device = thisDeviceId();
    const auto choiceAt = [&choices](qsizetype index) {
        return index < choices.size() ? choices.at(index).toString() : QString();
    };
    if (itemId.startsWith(vaultPrefix)) {
        QList<MergeNote> left;
        for (qsizetype index = 0; index < contents.vaultNotes.size(); ++index) {
            const MergeNote& note = contents.vaultNotes.at(index);
            const QString choice = choiceAt(index);
            if (groupOf(note) != itemId || choice.isEmpty()) {
                left.append(note);
                continue;
            }
            if (takesOther(note, choice, device)) {
                useOtherSide(contents, note);
            }
        }
        contents.vaultNotes = left;
        contentsChanged();
        saveInBackground(QString());
        return;
    }
    for (Item& item : contents.items) {
        if (item.id.toString(QUuid::WithoutBraces) != itemId || item.mergeNotes.isEmpty()) {
            continue;
        }
        QList<MergeNote> left;
        QList<MergeNote> keptBoth;
        bool gone = false;
        for (qsizetype index = 0; index < item.mergeNotes.size(); ++index) {
            const MergeNote& note = item.mergeNotes.at(index);
            const QString choice = choiceAt(index);
            if (note.kind != MergeNote::Kind::ChangedOnBoth || choice.isEmpty()) {
                left.append(note); // not chosen: it stays to be settled
                continue;
            }
            if (choice == QLatin1String("both")) {
                if (canKeepBoth(item, note)) {
                    keptBoth.append(note); // after the rest, so a name chosen here is its name too
                } else {
                    left.append(note);
                }
                continue;
            }
            gone = settleNote(item, note, takesOther(note, choice, device), now) || gone;
        }
        for (const MergeNote& note : std::as_const(keptBoth)) {
            if (keepBothValues(item, note, now)) {
                item.modified = now;
            } else {
                left.append(note);
            }
        }
        item.mergeNotes = left;
        if (gone) {
            removeForGood({item.id}, now);
        }
        contentsChanged();
        saveInBackground(gone ? QString() : itemId);
        return;
    }
}

// Items deleted for good, their deletion remembered so it reaches the other
// devices (D-054).
void VaultController::removeForGood(const QList<QUuid>& ids, const QDateTime& now)
{
    for (const QUuid& id : ids) {
        contents.items.removeIf([&id](const Item& one) { return one.id == id; });
        if (std::none_of(contents.deletedItems.cbegin(), contents.deletedItems.cend(),
                         [&id](const DeletedItem& one) { return one.id == id; })) {
            contents.deletedItems.append(DeletedItem{id, now});
        }
    }
}

} // namespace chrono_vault
