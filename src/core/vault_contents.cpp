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

// --- Version ------------------------------------------------------------
// vault_contents.cpp 0.0.14

// --- Version history ----------------------------------------------------
// v0.0.14: The contents format 3: categories and templates of the person's own, tags, favourites, attachments;
//          the names of the new templates' fields (D-027, D-028, D-029)
// v0.0.4: An item's own icon, and the contents format 2
// v0.0.2: Initial release

namespace chrono_vault {

namespace {

// Format 2 added the item's icon (D-018); format 3 the categories and
// templates, tags, favourites and attachments (D-027 to D-029). Formats 1 and
// 2 are still read.
constexpr int contentsFormat = 3;
constexpr int oldestContentsFormat = 1;
constexpr qsizetype largestIconBytes = 256 * 1024;
constexpr qsizetype largestItemCount = 100000;
constexpr qsizetype largestListCount = 1000;
constexpr qsizetype largestTextLength = 1000;

const QString keyFormat = QStringLiteral("format");
const QString keyItems = QStringLiteral("items");

QString kindName(FieldKind kind)
{
    switch (kind) {
    case FieldKind::Text: return QStringLiteral("text");
    case FieldKind::Password: return QStringLiteral("password");
    case FieldKind::WebAddress: return QStringLiteral("url");
    case FieldKind::OneTimeCode: return QStringLiteral("totp");
    case FieldKind::MultiLine: return QStringLiteral("multiline");
    }
    return QStringLiteral("text");
}

std::optional<FieldKind> kindFromName(const QString& name)
{
    for (const FieldKind kind : {FieldKind::Text, FieldKind::Password, FieldKind::WebAddress,
                                 FieldKind::OneTimeCode, FieldKind::MultiLine}) {
        if (kindName(kind) == name) {
            return kind;
        }
    }
    return std::nullopt;
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
    return item;
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
    };
    const auto found = names.constFind(role);
    return found == names.constEnd() ? role : QCoreApplication::translate("Field", found.value());
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
            categories.append(QJsonObject{{QStringLiteral("id"), category.id}, {QStringLiteral("name"), category.name}});
        }
        root.insert(QStringLiteral("categories"), categories);
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
            const QJsonObject object = value.toObject();
            if (!value.isObject() || !isString(object, "id") || !isString(object, "name") ||
                text(object, "id").isEmpty() || text(object, "id").size() > largestTextLength) {
                return result;
            }
            contents.categories.append(Category{text(object, "id"), text(object, "name")});
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
    result.status = ContentsStatus::Read;
    result.contents = contents;
    return result;
}

} // namespace chrono_vault
