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

#include "vault_contents.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <utility>

// --- Version ------------------------------------------------------------
// vault_contents.cpp 0.5.77

// --- Version history ----------------------------------------------------
// v0.5.77: A note's other value marked as imported, a key earlier versions pass over (D-060)
// v0.2.56: The contents format 8: a category's icon, the changes to built-in categories (D-056)
// v0.2.45: The contents format 7: notes a merge leaves on an item, items deleted for good (D-054)
// v0.1.41: The contents format 6: the reference program's other field types (D-051)
// v0.0.40: The contents format 5: the vault's own icons (D-050)
// v0.0.17: The contents format 4: sections, the reference program's field types, hide marks, hidden and deleted
//          fields, field history and ids, an item's subtitle and note, trash, archive, autosubmit (D-033)
// v0.0.15: The name of the field that holds a vault's file, for its password saved in the primary (D-030)
// v0.0.14: The contents format 3: categories and templates of the person's own, tags, favourites, attachments;
//          the names of the new templates' fields (D-027, D-028, D-029)
// v0.0.4: An item's own icon, and the contents format 2
// v0.0.2: Initial release

namespace chrono_vault {

namespace {

// Format 2 added the item's icon (D-018); format 3 the categories and
// templates, tags, favourites and attachments (D-027 to D-029); format 4
// what the reference program's items hold (D-033); format 5 the vault's own
// icons (D-050); format 6 the reference program's other field types (D-051);
// format 7 merging (D-054); format 8 a category's icon and the changes to
// built-in categories (D-056).
// Older formats are read.
constexpr int contentsFormat = 8;
constexpr int oldestContentsFormat = 1;
constexpr qsizetype largestIconBytes = 256 * 1024;
constexpr qsizetype largestItemCount = 100000;
constexpr qsizetype largestListCount = 1000;
constexpr qsizetype largestTextLength = 1000;

const QString keyFormat = QStringLiteral("format");
const QString keyItems = QStringLiteral("items");

constexpr FieldKind allKinds[] = {
    FieldKind::Text,     FieldKind::Password,   FieldKind::WebAddress, FieldKind::OneTimeCode, FieldKind::MultiLine,
    FieldKind::Email,    FieldKind::Username,   FieldKind::Phone,      FieldKind::Date,        FieldKind::Numeric,
    FieldKind::CardExpiry, FieldKind::CardCvc,  FieldKind::CardPin,    FieldKind::CardType,    FieldKind::Section,
    FieldKind::Hidden,   FieldKind::Pin,        FieldKind::CardName,   FieldKind::CardNumber,  FieldKind::CardBankName,
    FieldKind::CardTransactionPassword, FieldKind::CardValidFrom,
};

QString kindName(FieldKind kind)
{
    return fieldKindName(kind);
}

std::optional<FieldKind> kindFromName(const QString& name)
{
    return fieldKindFromName(name);
}

bool isString(const QJsonObject& object, const char* key)
{
    return object.value(QLatin1String(key)).isString();
}

QString text(const QJsonObject& object, const char* key)
{
    return object.value(QLatin1String(key)).toString();
}

QJsonArray fieldsToJson(const QList<Field>& fields)
{
    QJsonArray array;
    for (const Field& field : fields) {
        QJsonObject object{{QStringLiteral("kind"), kindName(field.kind)}, {QStringLiteral("value"), field.value}};
        if (field.isCustom()) {
            object.insert(QStringLiteral("label"), field.label);
        } else {
            object.insert(QStringLiteral("role"), field.role);
        }
        if (field.sensitive) {
            object.insert(QStringLiteral("sensitive"), true);
        }
        if (field.deleted) {
            object.insert(QStringLiteral("deleted"), true);
        }
        if (!field.hiddenType.isEmpty()) {
            object.insert(QStringLiteral("hiddenType"), field.hiddenType);
        }
        if (field.uid != 0) {
            object.insert(QStringLiteral("uid"), static_cast<double>(field.uid));
        }
        if (field.changed.isValid()) {
            object.insert(QStringLiteral("changed"), field.changed.toString(Qt::ISODateWithMs));
        }
        if (!field.history.isEmpty()) {
            QJsonArray history;
            for (const FieldChange& change : field.history) {
                history.append(QJsonObject{{QStringLiteral("value"), change.value},
                                           {QStringLiteral("changed"), change.changed.toString(Qt::ISODateWithMs)}});
            }
            object.insert(QStringLiteral("history"), history);
        }
        array.append(object);
    }
    return array;
}

// Nothing when one field is of the wrong shape: the whole is refused (OR-4).
std::optional<QList<Field>> fieldsFromJson(const QJsonValue& value)
{
    if (!value.isArray()) {
        return std::nullopt;
    }
    const QJsonArray array = value.toArray();
    if (array.size() > largestListCount) {
        return std::nullopt;
    }
    QList<Field> fields;
    fields.reserve(array.size());
    for (const QJsonValue& fieldValue : array) {
        if (!fieldValue.isObject()) {
            return std::nullopt;
        }
        const QJsonObject object = fieldValue.toObject();
        const auto kind = kindFromName(text(object, "kind"));
        const bool hasRole = isString(object, "role");
        const bool hasLabel = isString(object, "label");
        if (!kind || !isString(object, "value") || hasRole == hasLabel) {
            return std::nullopt;
        }
        Field field;
        field.kind = *kind;
        field.value = text(object, "value");
        if (hasRole) {
            field.role = text(object, "role");
            if (field.role.isEmpty()) {
                return std::nullopt;
            }
        } else {
            field.label = text(object, "label");
        }
        // Format 4 (D-033); each optional, each of its own type or refused.
        if (object.contains(QStringLiteral("sensitive"))) {
            if (!object.value(QStringLiteral("sensitive")).isBool()) {
                return std::nullopt;
            }
            field.sensitive = object.value(QStringLiteral("sensitive")).toBool();
        }
        if (object.contains(QStringLiteral("deleted"))) {
            if (!object.value(QStringLiteral("deleted")).isBool()) {
                return std::nullopt;
            }
            field.deleted = object.value(QStringLiteral("deleted")).toBool();
        }
        if (object.contains(QStringLiteral("hiddenType"))) {
            if (!isString(object, "hiddenType") || text(object, "hiddenType").size() > largestTextLength) {
                return std::nullopt;
            }
            field.hiddenType = text(object, "hiddenType");
        }
        if (object.contains(QStringLiteral("uid"))) {
            const QJsonValue uid = object.value(QStringLiteral("uid"));
            if (!uid.isDouble() || uid.toDouble() < 0 || uid.toDouble() > 9.0e15) {
                return std::nullopt;
            }
            field.uid = static_cast<qint64>(uid.toDouble());
        }
        if (object.contains(QStringLiteral("changed"))) {
            if (!isString(object, "changed")) {
                return std::nullopt;
            }
            field.changed = QDateTime::fromString(text(object, "changed"), Qt::ISODateWithMs);
        }
        if (object.contains(QStringLiteral("history"))) {
            const QJsonValue history = object.value(QStringLiteral("history"));
            if (!history.isArray() || history.toArray().size() > largestListCount) {
                return std::nullopt;
            }
            for (const QJsonValue& entry : history.toArray()) {
                const QJsonObject change = entry.toObject();
                if (!entry.isObject() || !isString(change, "value") || !isString(change, "changed")) {
                    return std::nullopt;
                }
                field.history.append(FieldChange{text(change, "value"),
                                                 QDateTime::fromString(text(change, "changed"), Qt::ISODateWithMs)});
            }
        }
        fields.append(field);
    }
    return fields;
}

std::optional<QByteArray> bytesFromBase64(const QJsonValue& value, qsizetype largest)
{
    if (!value.isString() || value.toString().size() > (largest / 3 + 1) * 4) {
        return std::nullopt;
    }
    const auto decoded =
        QByteArray::fromBase64Encoding(value.toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.size() > largest) {
        return std::nullopt;
    }
    return decoded.decoded;
}

QJsonObject itemToJson(const Item& item)
{
    QJsonObject object{
        {QStringLiteral("id"), item.id.toString(QUuid::WithoutBraces)},
        {QStringLiteral("template"), item.templateId},
        {QStringLiteral("category"), item.categoryId},
        {QStringLiteral("title"), item.title},
        {QStringLiteral("created"), item.created.toString(Qt::ISODateWithMs)},
        {QStringLiteral("modified"), item.modified.toString(Qt::ISODateWithMs)},
        {QStringLiteral("fields"), fieldsToJson(item.fields)},
    };
    if (!item.icon.isEmpty()) {
        object.insert(QStringLiteral("icon"), QString::fromLatin1(item.icon.toBase64()));
    }
    if (!item.tags.isEmpty()) {
        object.insert(QStringLiteral("tags"), QJsonArray::fromStringList(item.tags));
    }
    if (item.favourite) {
        object.insert(QStringLiteral("favourite"), true);
    }
    const auto insertText = [&object](const char* key, const QString& value) {
        if (!value.isEmpty()) {
            object.insert(QLatin1String(key), value);
        }
    };
    insertText("subtitle", item.subtitle);
    insertText("note", item.note);
    insertText("website", item.website);
    for (const auto& [key, on] : {std::pair{"trashed", item.trashed}, std::pair{"archived", item.archived},
                                  std::pair{"autoSubmit", item.autoSubmit}}) {
        if (on) {
            object.insert(QLatin1String(key), true);
        }
    }
    if (!item.mergeNotes.isEmpty()) {
        QJsonArray notes;
        for (const MergeNote& note : item.mergeNotes) {
            QJsonObject one{
                {QStringLiteral("kind"), note.kind == MergeNote::Kind::DeletedElsewhere ? QStringLiteral("deleted")
                                                                                         : QStringLiteral("both")},
                {QStringLiteral("when"), note.when.toString(Qt::ISODateWithMs)},
            };
            if (note.fieldUid != 0) {
                one.insert(QStringLiteral("field"), QString::number(note.fieldUid));
            }
            if (!note.property.isEmpty()) {
                one.insert(QStringLiteral("property"), note.property);
            }
            if (!note.otherValue.isEmpty()) {
                one.insert(QStringLiteral("other"), note.otherValue);
            }
            if (note.imported) {
                // A key an earlier version passes over (D-060).
                one.insert(QStringLiteral("source"), QStringLiteral("import"));
            }
            notes.append(one);
        }
        object.insert(QStringLiteral("mergeNotes"), notes);
    }
    if (!item.attachments.isEmpty()) {
        QJsonArray attachments;
        for (const Attachment& attachment : item.attachments) {
            attachments.append(QJsonObject{
                {QStringLiteral("id"), attachment.id.toString(QUuid::WithoutBraces)},
                {QStringLiteral("name"), attachment.name},
                {QStringLiteral("data"), QString::fromLatin1(attachment.data.toBase64())},
            });
        }
        object.insert(QStringLiteral("attachments"), attachments);
    }
    return object;
}

std::optional<Item> itemFromJson(const QJsonValue& value, int format)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    if (!isString(object, "id") || !isString(object, "template") || !isString(object, "title") ||
        !isString(object, "created") || !isString(object, "modified")) {
        return std::nullopt;
    }
    Item item;
    item.id = QUuid::fromString(text(object, "id"));
    item.templateId = text(object, "template");
    if (item.id.isNull() || item.templateId.isEmpty() || item.templateId.size() > largestTextLength) {
        return std::nullopt;
    }
    if (format < 3) {
        // Before format 3 there were two templates, each its own category.
        if (item.templateId != QLatin1String("login") && item.templateId != QLatin1String("note")) {
            return std::nullopt;
        }
        item.categoryId = item.templateId;
    } else {
        if (!isString(object, "category")) {
            return std::nullopt;
        }
        item.categoryId = text(object, "category");
        if (item.categoryId.isEmpty() || item.categoryId.size() > largestTextLength) {
            return std::nullopt;
        }
    }
    item.title = text(object, "title");
    item.created = QDateTime::fromString(text(object, "created"), Qt::ISODateWithMs);
    item.modified = QDateTime::fromString(text(object, "modified"), Qt::ISODateWithMs);
    if (object.contains(QStringLiteral("icon"))) {
        const auto icon = bytesFromBase64(object.value(QStringLiteral("icon")), largestIconBytes);
        if (!icon) {
            return std::nullopt;
        }
        item.icon = *icon;
    }
    const auto fields = fieldsFromJson(object.value(QStringLiteral("fields")));
    if (!fields) {
        return std::nullopt;
    }
    item.fields = *fields;

    if (object.contains(QStringLiteral("tags"))) {
        const QJsonValue tags = object.value(QStringLiteral("tags"));
        if (!tags.isArray() || tags.toArray().size() > largestListCount) {
            return std::nullopt;
        }
        for (const QJsonValue& tag : tags.toArray()) {
            const QString kept = normalizedTag(tag.toString());
            if (!tag.isString() || kept.isEmpty() || kept.size() > largestTextLength) {
                return std::nullopt;
            }
            if (!item.tags.contains(kept)) {
                item.tags.append(kept);
            }
        }
    }
    if (object.contains(QStringLiteral("favourite"))) {
        if (!object.value(QStringLiteral("favourite")).isBool()) {
            return std::nullopt;
        }
        item.favourite = object.value(QStringLiteral("favourite")).toBool();
    }
    for (const char* key : {"subtitle", "note", "website"}) {
        if (!object.contains(QLatin1String(key))) {
            continue;
        }
        if (format < 4 || !isString(object, key)) {
            return std::nullopt;
        }
        const QString given = text(object, key);
        if (qstrcmp(key, "subtitle") == 0) {
            item.subtitle = given;
        } else if (qstrcmp(key, "note") == 0) {
            item.note = given;
        } else {
            item.website = given;
        }
    }
    for (const char* key : {"trashed", "archived", "autoSubmit"}) {
        if (!object.contains(QLatin1String(key))) {
            continue;
        }
        if (format < 4 || !object.value(QLatin1String(key)).isBool()) {
            return std::nullopt;
        }
        const bool on = object.value(QLatin1String(key)).toBool();
        if (qstrcmp(key, "trashed") == 0) {
            item.trashed = on;
        } else if (qstrcmp(key, "archived") == 0) {
            item.archived = on;
        } else {
            item.autoSubmit = on;
        }
    }
    if (format < 4) {
        giveFieldsIds(item); // ids for the field history from now on
    }
    if (object.contains(QStringLiteral("attachments"))) {
        const QJsonValue attachments = object.value(QStringLiteral("attachments"));
        if (!attachments.isArray() || attachments.toArray().size() > largestListCount) {
            return std::nullopt;
        }
        for (const QJsonValue& attachmentValue : attachments.toArray()) {
            const QJsonObject attachmentObject = attachmentValue.toObject();
            if (!attachmentValue.isObject() || !isString(attachmentObject, "id") ||
                !isString(attachmentObject, "name")) {
                return std::nullopt;
            }
            Attachment attachment;
            attachment.id = QUuid::fromString(text(attachmentObject, "id"));
            attachment.name = text(attachmentObject, "name");
            const auto data = bytesFromBase64(attachmentObject.value(QStringLiteral("data")), largestAttachmentBytes);
            if (attachment.id.isNull() || attachment.name.isEmpty() || !data) {
                return std::nullopt;
            }
            attachment.data = *data;
            item.attachments.append(attachment);
        }
    }
    // Format 7 (D-054): what a merge left for the person to see.
    if (object.contains(QStringLiteral("mergeNotes"))) {
        const QJsonValue notes = object.value(QStringLiteral("mergeNotes"));
        if (format < 7 || !notes.isArray() || notes.toArray().size() > largestListCount) {
            return std::nullopt;
        }
        for (const QJsonValue& noteValue : notes.toArray()) {
            const QJsonObject noteObject = noteValue.toObject();
            if (!noteValue.isObject() || !isString(noteObject, "kind") || !isString(noteObject, "when")) {
                return std::nullopt;
            }
            MergeNote note;
            const QString kind = text(noteObject, "kind");
            if (kind == QLatin1String("deleted")) {
                note.kind = MergeNote::Kind::DeletedElsewhere;
            } else if (kind != QLatin1String("both")) {
                return std::nullopt;
            }
            note.when = QDateTime::fromString(text(noteObject, "when"), Qt::ISODateWithMs);
            if (noteObject.contains(QStringLiteral("field"))) {
                bool number = false;
                note.fieldUid = text(noteObject, "field").toLongLong(&number);
                if (!number) {
                    return std::nullopt;
                }
            }
            note.property = text(noteObject, "property");
            note.otherValue = text(noteObject, "other");
            note.imported = text(noteObject, "source") == QLatin1String("import");
            if (note.property.size() > largestTextLength) {
                return std::nullopt;
            }
            item.mergeNotes.append(note);
        }
    }
    return item;
}

// A category: its id, its name and, from format 8, its icon (D-056). A
// change to a built-in one may leave the name empty: the program's own.
QJsonObject categoryToJson(const Category& category)
{
    QJsonObject object{{QStringLiteral("id"), category.id}, {QStringLiteral("name"), category.name}};
    if (!category.icon.isEmpty()) {
        object.insert(QStringLiteral("icon"), QString::fromLatin1(category.icon.toBase64()));
    }
    return object;
}

std::optional<Category> categoryFromJson(const QJsonValue& value, int format)
{
    const QJsonObject object = value.toObject();
    if (!value.isObject() || !isString(object, "id") || !isString(object, "name") || text(object, "id").isEmpty() ||
        text(object, "id").size() > largestTextLength) {
        return std::nullopt;
    }
    Category category{text(object, "id"), text(object, "name"), {}};
    if (object.contains(QStringLiteral("icon"))) {
        const auto icon = bytesFromBase64(object.value(QStringLiteral("icon")), largestIconBytes);
        if (format < 8 || !icon || icon->isEmpty()) {
            return std::nullopt;
        }
        category.icon = *icon;
    }
    return category;
}

} // namespace

QString roleName(const QString& role)
{
    static const QHash<QString, const char*> names{
        {QStringLiteral("username"), QT_TRANSLATE_NOOP("Field", "User name")},
        {QStringLiteral("password"), QT_TRANSLATE_NOOP("Field", "Password")},
        {QStringLiteral("url"), QT_TRANSLATE_NOOP("Field", "Web address")},
        {QStringLiteral("totp"), QT_TRANSLATE_NOOP("Field", "One-time code")},
        {QStringLiteral("notes"), QT_TRANSLATE_NOOP("Field", "Notes")},
        {QStringLiteral("email"), QT_TRANSLATE_NOOP("Field", "Email address")},
        {QStringLiteral("server"), QT_TRANSLATE_NOOP("Field", "Server")},
        {QStringLiteral("cardholder"), QT_TRANSLATE_NOOP("Field", "Cardholder")},
        {QStringLiteral("cardnumber"), QT_TRANSLATE_NOOP("Field", "Card number")},
        {QStringLiteral("expiry"), QT_TRANSLATE_NOOP("Field", "Expiry date")},
        {QStringLiteral("cvc"), QT_TRANSLATE_NOOP("Field", "Security code")},
        {QStringLiteral("pin"), QT_TRANSLATE_NOOP("Field", "PIN")},
        {QStringLiteral("fullname"), QT_TRANSLATE_NOOP("Field", "Full name")},
        {QStringLiteral("birthday"), QT_TRANSLATE_NOOP("Field", "Date of birth")},
        {QStringLiteral("address"), QT_TRANSLATE_NOOP("Field", "Address")},
        {QStringLiteral("phone"), QT_TRANSLATE_NOOP("Field", "Phone")},
        {QStringLiteral("number"), QT_TRANSLATE_NOOP("Field", "Number")},
        {QStringLiteral("nationality"), QT_TRANSLATE_NOOP("Field", "Nationality")},
        {QStringLiteral("issued"), QT_TRANSLATE_NOOP("Field", "Issued on")},
        {QStringLiteral("expires"), QT_TRANSLATE_NOOP("Field", "Expires on")},
        {QStringLiteral("bankname"), QT_TRANSLATE_NOOP("Field", "Bank")},
        {QStringLiteral("accountholder"), QT_TRANSLATE_NOOP("Field", "Account holder")},
        {QStringLiteral("accountnumber"), QT_TRANSLATE_NOOP("Field", "Account number")},
        {QStringLiteral("iban"), QT_TRANSLATE_NOOP("Field", "IBAN")},
        {QStringLiteral("bic"), QT_TRANSLATE_NOOP("Field", "BIC")},
        {QStringLiteral("network"), QT_TRANSLATE_NOOP("Field", "Network name")},
        {QStringLiteral("security"), QT_TRANSLATE_NOOP("Field", "Security")},
        {QStringLiteral("product"), QT_TRANSLATE_NOOP("Field", "Product")},
        {QStringLiteral("licencekey"), QT_TRANSLATE_NOOP("Field", "License key")},
        {QStringLiteral("licensedto"), QT_TRANSLATE_NOOP("Field", "Licensed to")},
        {QStringLiteral("organisation"), QT_TRANSLATE_NOOP("Field", "Organization")},
        {QStringLiteral("membernumber"), QT_TRANSLATE_NOOP("Field", "Member number")},
        {QStringLiteral("vaultfile"), QT_TRANSLATE_NOOP("Field", "Vault file")},
    };
    const auto found = names.constFind(role);
    return found == names.constEnd() ? role : QCoreApplication::translate("Field", found.value());
}

void giveFieldsIds(Item& item)
{
    qint64 highest = 0;
    for (const Field& field : std::as_const(item.fields)) {
        highest = std::max(highest, field.uid);
    }
    for (Field& field : item.fields) {
        if (field.uid == 0) {
            field.uid = ++highest;
        }
    }
}

QString fieldKindName(FieldKind kind)
{
    switch (kind) {
    case FieldKind::Text: return QStringLiteral("text");
    case FieldKind::Password: return QStringLiteral("password");
    case FieldKind::WebAddress: return QStringLiteral("url");
    case FieldKind::OneTimeCode: return QStringLiteral("totp");
    case FieldKind::MultiLine: return QStringLiteral("multiline");
    case FieldKind::Email: return QStringLiteral("email");
    case FieldKind::Username: return QStringLiteral("username");
    case FieldKind::Phone: return QStringLiteral("phone");
    case FieldKind::Date: return QStringLiteral("date");
    case FieldKind::Numeric: return QStringLiteral("numeric");
    case FieldKind::CardExpiry: return QStringLiteral("ccExpiry");
    case FieldKind::CardCvc: return QStringLiteral("ccCvc");
    case FieldKind::CardPin: return QStringLiteral("ccPin");
    case FieldKind::CardType: return QStringLiteral("ccType");
    case FieldKind::Section: return QStringLiteral("section");
    case FieldKind::Hidden: return QStringLiteral("hidden");
    case FieldKind::Pin: return QStringLiteral("pin");
    case FieldKind::CardName: return QStringLiteral("ccName");
    case FieldKind::CardNumber: return QStringLiteral("ccNumber");
    case FieldKind::CardBankName: return QStringLiteral("ccBankname");
    case FieldKind::CardTransactionPassword: return QStringLiteral("ccTxnpassword");
    case FieldKind::CardValidFrom: return QStringLiteral("ccValidfrom");
    }
    return QStringLiteral("text");
}

std::optional<FieldKind> fieldKindFromName(const QString& name)
{
    for (const FieldKind kind : allKinds) {
        if (fieldKindName(kind) == name) {
            return kind;
        }
    }
    return std::nullopt;
}

QString normalizedTag(const QString& tag)
{
    QStringList parts;
    for (const QString& part : tag.split(QLatin1Char('/'))) {
        const QString trimmed = part.simplified();
        if (!trimmed.isEmpty()) {
            parts.append(trimmed);
        }
    }
    return parts.join(QLatin1Char('/'));
}

QByteArray contentsToJson(const VaultContents& contents)
{
    QJsonArray items;
    for (const Item& item : contents.items) {
        items.append(itemToJson(item));
    }
    QJsonObject root{{keyFormat, contentsFormat}, {keyItems, items}};
    if (!contents.categories.isEmpty()) {
        QJsonArray categories;
        for (const Category& category : contents.categories) {
            categories.append(categoryToJson(category));
        }
        root.insert(QStringLiteral("categories"), categories);
    }
    if (!contents.builtInCategoryChanges.isEmpty()) {
        QJsonArray changes;
        for (const Category& category : contents.builtInCategoryChanges) {
            changes.append(categoryToJson(category));
        }
        root.insert(QStringLiteral("builtInCategories"), changes);
    }
    if (!contents.templates.isEmpty()) {
        QJsonArray templates;
        for (const Template& one : contents.templates) {
            templates.append(QJsonObject{
                {QStringLiteral("id"), one.id},
                {QStringLiteral("category"), one.categoryId},
                {QStringLiteral("name"), one.name},
                {QStringLiteral("fields"), fieldsToJson(one.fields)},
            });
        }
        root.insert(QStringLiteral("templates"), templates);
    }
    if (!contents.hiddenCategories.isEmpty()) {
        root.insert(QStringLiteral("hiddenCategories"), QJsonArray::fromStringList(contents.hiddenCategories));
    }
    if (!contents.customIcons.isEmpty()) {
        QJsonArray icons;
        for (const QByteArray& icon : contents.customIcons) {
            icons.append(QString::fromLatin1(icon.toBase64()));
        }
        root.insert(QStringLiteral("customIcons"), icons);
    }
    if (!contents.deletedItems.isEmpty()) {
        QJsonArray deleted;
        for (const DeletedItem& one : contents.deletedItems) {
            deleted.append(QJsonObject{{QStringLiteral("id"), one.id.toString(QUuid::WithoutBraces)},
                                       {QStringLiteral("when"), one.when.toString(Qt::ISODateWithMs)}});
        }
        root.insert(QStringLiteral("deletedItems"), deleted);
    }
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

ContentsResult contentsFromJson(const QByteArray& json)
{
    ContentsResult result;
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return result;
    }
    const QJsonObject root = document.object();
    const QJsonValue formatValue = root.value(keyFormat);
    if (!formatValue.isDouble()) {
        return result;
    }
    const int format = formatValue.toInt();
    if (format > contentsFormat) {
        result.status = ContentsStatus::NewerFormat;
        return result;
    }
    if (format < oldestContentsFormat || !root.value(keyItems).isArray()) {
        return result;
    }
    const QJsonArray items = root.value(keyItems).toArray();
    if (items.size() > largestItemCount) {
        return result;
    }

    VaultContents contents;
    contents.items.reserve(items.size());
    for (const QJsonValue& itemValue : items) {
        const auto item = itemFromJson(itemValue, format);
        if (!item) {
            return result;
        }
        contents.items.append(*item);
    }

    if (root.contains(QStringLiteral("categories"))) {
        const QJsonValue categories = root.value(QStringLiteral("categories"));
        if (format < 3 || !categories.isArray() || categories.toArray().size() > largestListCount) {
            return result;
        }
        for (const QJsonValue& value : categories.toArray()) {
            const auto category = categoryFromJson(value, format);
            if (!category) {
                return result;
            }
            contents.categories.append(*category);
        }
    }
    // Format 8 (D-056): the built-in categories as the person changed them.
    if (root.contains(QStringLiteral("builtInCategories"))) {
        const QJsonValue changes = root.value(QStringLiteral("builtInCategories"));
        if (format < 8 || !changes.isArray() || changes.toArray().size() > largestListCount) {
            return result;
        }
        for (const QJsonValue& value : changes.toArray()) {
            const auto category = categoryFromJson(value, format);
            if (!category) {
                return result;
            }
            contents.builtInCategoryChanges.append(*category);
        }
    }
    if (root.contains(QStringLiteral("templates"))) {
        const QJsonValue templates = root.value(QStringLiteral("templates"));
        if (format < 3 || !templates.isArray() || templates.toArray().size() > largestListCount) {
            return result;
        }
        for (const QJsonValue& value : templates.toArray()) {
            const QJsonObject object = value.toObject();
            if (!value.isObject() || !isString(object, "id") || !isString(object, "category") ||
                !isString(object, "name") || text(object, "id").isEmpty() ||
                text(object, "id").size() > largestTextLength || text(object, "category").isEmpty()) {
                return result;
            }
            const auto fields = fieldsFromJson(object.value(QStringLiteral("fields")));
            if (!fields) {
                return result;
            }
            contents.templates.append(
                Template{text(object, "id"), text(object, "category"), text(object, "name"), *fields, false});
        }
    }
    if (root.contains(QStringLiteral("hiddenCategories"))) {
        const QJsonValue hidden = root.value(QStringLiteral("hiddenCategories"));
        if (format < 3 || !hidden.isArray() || hidden.toArray().size() > largestListCount) {
            return result;
        }
        for (const QJsonValue& value : hidden.toArray()) {
            if (!value.isString()) {
                return result;
            }
            contents.hiddenCategories.append(value.toString());
        }
    }
    // Format 5 (D-050): the vault's own icons, each a PNG no larger than an
    // item's icon.
    if (root.contains(QStringLiteral("customIcons"))) {
        const QJsonValue icons = root.value(QStringLiteral("customIcons"));
        if (format < 5 || !icons.isArray() || icons.toArray().size() > largestListCount) {
            return result;
        }
        for (const QJsonValue& value : icons.toArray()) {
            const auto icon = bytesFromBase64(value, largestIconBytes);
            if (!icon || icon->isEmpty()) {
                return result;
            }
            contents.customIcons.append(*icon);
        }
    }
    // Format 7 (D-054): the items deleted for good.
    if (root.contains(QStringLiteral("deletedItems"))) {
        const QJsonValue deleted = root.value(QStringLiteral("deletedItems"));
        if (format < 7 || !deleted.isArray() || deleted.toArray().size() > largestItemCount * 10) {
            return result;
        }
        for (const QJsonValue& value : deleted.toArray()) {
            const QJsonObject object = value.toObject();
            if (!value.isObject() || !isString(object, "id") || !isString(object, "when")) {
                return result;
            }
            DeletedItem one{QUuid::fromString(text(object, "id")),
                            QDateTime::fromString(text(object, "when"), Qt::ISODateWithMs)};
            if (one.id.isNull()) {
                return result;
            }
            contents.deletedItems.append(one);
        }
    }
    if (format < 5) {
        // A vault from before its own icons were kept: the icons its items
        // carry become them, each once (D-050).
        for (const Item& item : std::as_const(contents.items)) {
            if (!item.icon.isEmpty() && !contents.customIcons.contains(item.icon)) {
                contents.customIcons.append(item.icon);
            }
        }
    }
    result.status = ContentsStatus::Read;
    result.contents = contents;
    return result;
}

} // namespace chrono_vault
