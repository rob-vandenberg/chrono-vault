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

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUuid>

#include <optional>

// --- Version ------------------------------------------------------------
// vault_contents.h 0.5.81

// --- Version history ----------------------------------------------------
// v0.5.81: MergeNote::source, and what its subject and property say of a field never there and of a deletion (D-061)
// v0.6.86: The contents format 10: an item left out of the audit (D-065)
// v0.5.78: The contents format 9: every difference a merge or an import meets kept for the person to settle (D-061)
// v0.5.77: MergeNote::imported (D-060)
// v0.2.56: The contents format 8: a category's icon, the changes to built-in categories (D-056)
// v0.2.45: The contents format 7: notes a merge leaves on an item, items deleted for good (D-054)
// v0.1.41: The reference program's other field types (D-051)
// v0.0.40: The vault's own icons (D-050)
// v0.0.17: Sections, the reference program's field types, a hide mark on any field, hidden and deleted fields, field
//          history and ids; an item's own subtitle and note, trash, archive and autosubmit; contents format 4 (D-033)
// v0.0.14: Categories and templates of the person's own, tags, favourites and attachments; the contents format 3
//          (D-027, D-028, D-029)
// v0.0.4: An item's own icon
// v0.0.2: Initial release

namespace chrono_vault {

// Every kind of field the reference program has (D-033). Section is a
// heading between fields; Hidden is a field the program keeps but never
// shows, such as the reference program's types starting with a dot.
enum class FieldKind {
    Text,
    Password,
    WebAddress,
    OneTimeCode,
    MultiLine,
    Email,
    Username,
    Phone,
    Date,
    Numeric,
    CardExpiry,
    CardCvc,
    CardPin,
    CardType,
    Section,
    Hidden,
    // The reference program's other types (D-051).
    Pin,
    CardName,
    CardNumber,
    CardBankName,
    CardTransactionPassword,
    CardValidFrom,
};

// An earlier value of a field, kept when the value changed (D-033).
struct FieldChange
{
    QString value;
    QDateTime changed;

    friend bool operator==(const FieldChange&, const FieldChange&) = default;
};

struct Field
{
    QString role;  // a built-in field's role, such as "username"; empty for a custom field
    QString label; // the name of a custom field, or a section's heading
    FieldKind kind = FieldKind::Text;
    QString value;
    bool sensitive = false; // shown as dots (D-033); always so for a password
    bool deleted = false;   // kept as the reference program keeps it, never shown
    QString hiddenType;     // a hidden field's own type, such as ".twoFA"
    qint64 uid = 0;         // the field's id within its item; 0 until it is saved
    QDateTime changed;      // when its value last changed
    QList<FieldChange> history; // earlier values, oldest first

    [[nodiscard]] bool isCustom() const { return role.isEmpty(); }
    [[nodiscard]] bool isHidden() const { return sensitive || kind == FieldKind::Password; }
    friend bool operator==(const Field&, const Field&) = default;
};

// A file carried by an item, sealed inside the vault like everything else
// (D-029).
struct Attachment
{
    QUuid id;
    QString name;
    QByteArray data;

    friend bool operator==(const Attachment&, const Attachment&) = default;
};

// What a merge left on an item for the person to see (D-054): a value
// changed differently on two devices (the newest stands, the other is kept
// here and in the field's history), or an item deleted on the other device
// but changed here, which therefore came back.
struct MergeNote
{
    enum class Kind { ChangedOnBoth, DeletedElsewhere };
    Kind kind = Kind::ChangedOnBoth;
    qint64 fieldUid = 0; // the field; 0 for the item itself
    // What differs (D-061). Of a field: "" its value, "present" (the field is
    // not on the item here; the other side has it, with otherValue),
    // "absent" (the other side does not have it), "label", "hidden" ("1" or
    // "0"), "history" (an earlier value only the other side holds, from
    // otherWhen). Of the item: "title", "subtitle", "note", "website",
    // "category", "template", "trashed", "archived", "favourite",
    // "autoSubmit" ("1" or "0"), "icon" (otherData), "tag" (a tag only the
    // other side holds), "attachment" (subject: its id; otherValue its name,
    // otherData its content), "attachmentAbsent" (subject: its id; the other
    // side does not have it), "deletedHere" (the item was deleted for good
    // here and the other side still holds it). Of the vault (in
    // VaultContents::vaultNotes): "category", "builtInCategory" (subject: its
    // id; otherValue its name, otherData its icon) and "template" (subject:
    // its id; otherData the other template, as templateToJson makes it).
    QString property;
    QString otherValue;  // the value that did not stand
    QDateTime when;      // when the merge was made
    bool imported = false; // the other value came from an import (D-060)
    QString subject;       // what a note of an attachment or of the vault is about (D-061)
    QByteArray otherData;  // an icon, an attachment's content or a template (D-061)
    QDateTime otherWhen;   // an earlier value's time (D-061)
    QString device;        // the device whose value stands; empty for an import (D-061)
    // Of a "present" note: subject "never" when this side never had the
    // field, so it reads "(not on the item)" and not "(deleted)". Of an item
    // deleted on one side: property "" with the device that changed it, or
    // "deletedOn" with the device that deleted it; no device in an older one.
    // Where an import's value came from, so Retry sync can read it again:
    // "export:" or "vault:" and the file. Not part of what differs.
    QString source;

    friend bool operator==(const MergeNote&, const MergeNote&) = default;
};

// An item deleted for good, remembered so the deletion reaches the other
// devices (D-054). Kept for good: about 30 bytes each.
struct DeletedItem
{
    QUuid id;
    QDateTime when;

    friend bool operator==(const DeletedItem&, const DeletedItem&) = default;
};

struct Item
{
    QUuid id;
    QString templateId = QStringLiteral("login"); // the template it was made from (D-027)
    QString categoryId = QStringLiteral("login");
    QString title;
    QString subtitle; // its own second line; empty for the one Chrono Vault finds (D-033)
    QString note;     // the item's own note, apart from its fields (D-033)
    QList<Field> fields;
    QStringList tags; // each tag nested with "/", such as "Work/Servers" (D-028)
    bool favourite = false;
    bool trashed = false;     // in the Trash (D-033)
    bool archived = false;    // in the Archive (D-033)
    bool noAudit = false;     // left out of the audit (D-065)
    bool autoSubmit = false;  // the reference program's "Autosubmit Login", kept (D-033)
    QString website;          // the site whose icon is shown when website icons are on (D-033)
    QList<Attachment> attachments;
    QByteArray icon; // PNG, 64 by 64 at most; empty for the category's own icon (D-018)
    QDateTime created;
    QDateTime modified;
    QList<MergeNote> mergeNotes; // shown on the item until the person settles them (D-054)

    friend bool operator==(const Item&, const Item&) = default;
};

// A category the person made (D-027). The built-in ones are in templates.h;
// what the person changed of one is kept as a Category with its id, an empty
// name or icon meaning the program's own (D-056).
struct Category
{
    QString id;
    QString name;
    QByteArray icon; // PNG, as an item's; empty for the program's own (format 8, D-056)

    friend bool operator==(const Category&, const Category&) = default;
};

// A template: the fields a new item starts with. The person's own are kept
// in the vault; the built-in ones are in templates.h (D-027).
struct Template
{
    QString id;
    QString categoryId;
    QString name;        // empty for a built-in template, whose name is translated
    QList<Field> fields; // the values are empty
    bool builtIn = false;

    friend bool operator==(const Template&, const Template&) = default;
};

struct VaultContents
{
    QList<Item> items;
    QList<Category> categories;     // the person's own
    QList<Template> templates;      // the person's own
    QStringList hiddenCategories;   // not shown in the left column
    // The vault's own icons, PNG each, offered in the icon picker; kept in
    // the vault so they come along to every device (D-050).
    QList<QByteArray> customIcons;
    QList<DeletedItem> deletedItems; // format 7 (D-054)
    QList<Category> builtInCategoryChanges; // format 8: the built-in ones as the person changed them (D-056)
    QList<MergeNote> vaultNotes; // format 9: the person's own categories and templates that differ (D-061)

    friend bool operator==(const VaultContents&, const VaultContents&) = default;
};

enum class ContentsStatus { Read, NewerFormat, Damaged };

struct ContentsResult
{
    ContentsStatus status = ContentsStatus::Damaged;
    VaultContents contents;
};

// The largest attachment, and the most all attachments of one vault hold
// together, so the vault file stays far below the size it may have (D-029).
inline constexpr qsizetype largestAttachmentBytes = 5 * 1024 * 1024;
inline constexpr qsizetype largestAttachmentsTotal = 150 * 1024 * 1024;

[[nodiscard]] QString roleName(const QString& role);

// A kind's name as the vault file and the windows use it ("text", "url",
// "ccExpiry" …), and back; nothing for a name that is no kind.
[[nodiscard]] QString fieldKindName(FieldKind kind);
[[nodiscard]] std::optional<FieldKind> fieldKindFromName(const QString& name);

// A field with only these set; the rest as a new field has them.
[[nodiscard]] inline Field makeField(const QString& role, const QString& label, FieldKind kind, const QString& value = {})
{
    Field field;
    field.role = role;
    field.label = label;
    field.kind = kind;
    field.value = value;
    field.sensitive = kind == FieldKind::Password || kind == FieldKind::CardPin || kind == FieldKind::Pin ||
                      kind == FieldKind::CardTransactionPassword;
    return field;
}

// Gives every field of the item that has none an id of its own (D-033).
void giveFieldsIds(Item& item);

// A tag as it is kept: every part trimmed, empty parts left out, joined by
// "/". Empty when nothing is left.
[[nodiscard]] QString normalizedTag(const QString& tag);

[[nodiscard]] QByteArray contentsToJson(const VaultContents& contents);

// A template on its own, as a vault note keeps the other side's (D-061).
[[nodiscard]] QByteArray templateToJson(const Template& one);
[[nodiscard]] std::optional<Template> templateFromJson(const QByteArray& json);

[[nodiscard]] ContentsResult contentsFromJson(const QByteArray& json);

} // namespace chrono_vault
