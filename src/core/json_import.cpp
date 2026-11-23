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
#include "json_import.h"

#include "templates.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>

#include <algorithm>

// --- Version ------------------------------------------------------------
// json_import.cpp 0.1.41

// --- Version history ----------------------------------------------------
// v0.1.41: The reference program's other field types read (D-051)
// v0.0.22: Chrono Vault's own built-in templates, as its export names them, come back as themselves (D-037)
// v0.0.19: Initial release (D-035)

namespace chrono_vault {

namespace {

constexpr qsizetype largestItemCount = 100000;
constexpr qsizetype largestListCount = 10000;
constexpr qsizetype largestIconBytes = 256 * 1024;

// The export's field types, as Chrono Vault's kinds. A type starting with a
// dot is a hidden field; a type it does not know is kept as text, value and
// all, so nothing is dropped.
FieldKind kindOf(const QString& type)
{
    static const QHash<QString, FieldKind> kinds{
        {QStringLiteral("text"), FieldKind::Text},         {QStringLiteral("password"), FieldKind::Password},
        {QStringLiteral("username"), FieldKind::Username}, {QStringLiteral("email"), FieldKind::Email},
        {QStringLiteral("url"), FieldKind::WebAddress},    {QStringLiteral("phone"), FieldKind::Phone},
        {QStringLiteral("date"), FieldKind::Date},         {QStringLiteral("numeric"), FieldKind::Numeric},
        {QStringLiteral("totp"), FieldKind::OneTimeCode},  {QStringLiteral("multiline"), FieldKind::MultiLine},
        {QStringLiteral("section"), FieldKind::Section},   {QStringLiteral("ccExpiry"), FieldKind::CardExpiry},
        {QStringLiteral("ccCvc"), FieldKind::CardCvc},     {QStringLiteral("ccPin"), FieldKind::CardPin},
        {QStringLiteral("ccType"), FieldKind::CardType},
        {QStringLiteral("pin"), FieldKind::Pin},           {QStringLiteral("ccName"), FieldKind::CardName},
        {QStringLiteral("ccNumber"), FieldKind::CardNumber}, {QStringLiteral("ccBankname"), FieldKind::CardBankName},
        {QStringLiteral("ccTxnpassword"), FieldKind::CardTransactionPassword},
        {QStringLiteral("ccValidfrom"), FieldKind::CardValidFrom},
    };
    if (type.startsWith(QLatin1Char('.'))) {
        return FieldKind::Hidden;
    }
    return kinds.value(type, FieldKind::Text);
}

// The export's own category names that are Chrono Vault's built-in ones.
QString builtInCategoryFor(const QString& name)
{
    static const QHash<QString, QString> names{
        {QStringLiteral("login"), QStringLiteral("login")},       {QStringLiteral("creditcard"), QStringLiteral("card")},
        {QStringLiteral("identity"), QStringLiteral("identity")}, {QStringLiteral("note"), QStringLiteral("note")},
        {QStringLiteral("password"), QStringLiteral("password")}, {QStringLiteral("finance"), QStringLiteral("finance")},
        {QStringLiteral("computer"), QStringLiteral("computer")}, {QStringLiteral("misc"), QStringLiteral("misc")},
    };
    return names.value(name);
}

// The export's own template names that are Chrono Vault's built-in ones.
QString builtInTemplateFor(const QString& name)
{
    static const QHash<QString, QString> names{
        {QStringLiteral("login.default"), QStringLiteral("login")},
        {QStringLiteral("note.default"), QStringLiteral("note")},
        {QStringLiteral("password.default"), QStringLiteral("password")},
        {QStringLiteral("creditcard.default"), QStringLiteral("card")},
        {QStringLiteral("identity.default"), QStringLiteral("identity")},
    };
    if (names.contains(name)) {
        return names.value(name);
    }
    // Chrono Vault's other built-in templates, as its own export names them.
    for (const Template& one : builtInTemplates()) {
        if (one.id == name) {
            return name;
        }
    }
    return {};
}

bool isInt(const QJsonValue& value)
{
    return value.isDouble() && value.toDouble() == static_cast<double>(static_cast<qint64>(value.toDouble()));
}

QDateTime timeOf(const QJsonValue& value)
{
    return isInt(value) ? QDateTime::fromSecsSinceEpoch(static_cast<qint64>(value.toDouble()), QTimeZone::UTC)
                        : QDateTime();
}

// Optional keys: missing is fine, of the wrong kind is damage.
bool optionalString(const QJsonObject& object, const char* key, QString& into)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isString()) {
        return false;
    }
    into = value.toString();
    return true;
}

bool optionalFlag(const QJsonObject& object, const char* key, bool& into)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (value.isBool()) {
        into = value.toBool();
        return true;
    }
    if (!isInt(value)) {
        return false;
    }
    into = value.toInt() != 0;
    return true;
}

std::optional<Field> fieldFrom(const QJsonValue& value)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    QString type;
    Field field;
    if (!optionalString(object, "type", type) || !optionalString(object, "label", field.label) ||
        !optionalString(object, "value", field.value) || !optionalFlag(object, "sensitive", field.sensitive) ||
        !optionalFlag(object, "deleted", field.deleted)) {
        return std::nullopt;
    }
    field.kind = kindOf(type);
    if (field.kind == FieldKind::Hidden) {
        field.hiddenType = type;
    }
    if (isInt(object.value(QStringLiteral("uid")))) {
        field.uid = std::max<qint64>(0, static_cast<qint64>(object.value(QStringLiteral("uid")).toDouble()));
    }
    field.changed = timeOf(object.value(QStringLiteral("value_updated_at")));
    const QJsonValue history = object.value(QStringLiteral("history"));
    if (!history.isUndefined() && !history.isNull()) {
        if (!history.isArray() || history.toArray().size() > largestListCount) {
            return std::nullopt;
        }
        QList<std::pair<QDateTime, QString>> older;
        for (const QJsonValue& entry : history.toArray()) {
            const QJsonObject change = entry.toObject();
            QString earlier;
            if (!entry.isObject() || !optionalString(change, "value", earlier)) {
                return std::nullopt;
            }
            older.append({timeOf(change.value(QStringLiteral("updated_at"))), earlier});
        }
        // Chrono Vault keeps the history oldest first (D-033).
        std::stable_sort(older.begin(), older.end(),
                         [](const auto& left, const auto& right) { return left.first < right.first; });
        for (const auto& [when, earlier] : older) {
            field.history.append(FieldChange{earlier, when});
        }
    }
    return field;
}

} // namespace

QString referenceProgramName()
{
    return QStringLiteral("Enpass");
}

namespace {

ImportResult readExport(const QByteArray& json, const VaultContents& existing)
{
    ImportResult result;
    if (json.size() > largestImportBytes) {
        result.status = ImportStatus::TooLarge;
        return result;
    }
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.status = ImportStatus::NotTheFormat;
        return result;
    }
    const QJsonObject root = document.object();
    const QJsonValue itemsValue = root.value(QStringLiteral("items"));
    if (!itemsValue.isArray()) {
        result.status = ImportStatus::NotTheFormat;
        return result;
    }
    const QJsonArray items = itemsValue.toArray();
    if (items.size() > largestItemCount) {
        result.status = ImportStatus::TooLarge;
        return result;
    }

    // The custom icons, by uuid; PNG written as hexadecimal.
    QHash<QString, QByteArray> icons;
    const QJsonValue iconList = root.value(QStringLiteral("custom_icons"));
    if (!iconList.isUndefined() && !iconList.isNull()) {
        if (!iconList.isArray() || iconList.toArray().size() > largestListCount) {
            return result;
        }
        for (const QJsonValue& value : iconList.toArray()) {
            const QJsonObject icon = value.toObject();
            QString uuid;
            QString hex;
            if (!value.isObject() || !optionalString(icon, "uuid", uuid) || !optionalString(icon, "data", hex) ||
                hex.size() > 2 * largestIconBytes) {
                return result;
            }
            const QByteArray png = QByteArray::fromHex(hex.toLatin1());
            if (!png.startsWith("\x89PNG")) {
                return result;
            }
            icons.insert(uuid, png);
        }
    }

    // Tags, when the export has them: folders, nested by their parent.
    QHash<QString, std::pair<QString, QString>> folders; // uuid: title, parent uuid
    const QJsonValue folderList = root.value(QStringLiteral("folders"));
    if (folderList.isArray()) {
        for (const QJsonValue& value : folderList.toArray()) {
            const QJsonObject folder = value.toObject();
            QString uuid;
            QString title;
            QString parent;
            if (!value.isObject() || !optionalString(folder, "uuid", uuid) || !optionalString(folder, "title", title) ||
                !optionalString(folder, "parent_uuid", parent)) {
                return result;
            }
            folders.insert(uuid, {title, parent});
        }
    }
    const auto tagPath = [&folders](QString uuid) {
        QStringList parts;
        for (int depth = 0; depth < 64 && folders.contains(uuid); ++depth) {
            parts.prepend(folders.value(uuid).first);
            uuid = folders.value(uuid).second;
        }
        return normalizedTag(parts.join(QLatin1Char('/')));
    };

    QHash<QString, QString> categoryIds;  // the export's category to Chrono Vault's
    QHash<QString, QString> templateIds;  // the export's template to Chrono Vault's
    const auto categoryKnown = [&existing, &result](const QString& id) {
        return std::any_of(existing.categories.cbegin(), existing.categories.cend(),
                           [&id](const Category& one) { return one.id == id; }) ||
               std::any_of(result.categories.cbegin(), result.categories.cend(),
                           [&id](const Category& one) { return one.id == id; });
    };
    const auto templateKnown = [&existing, &result](const QString& id) {
        return std::any_of(existing.templates.cbegin(), existing.templates.cend(),
                           [&id](const Template& one) { return one.id == id; }) ||
               std::any_of(result.templates.cbegin(), result.templates.cend(),
                           [&id](const Template& one) { return one.id == id; });
    };

    for (const QJsonValue& itemValue : items) {
        if (!itemValue.isObject()) {
            return result;
        }
        const QJsonObject object = itemValue.toObject();
        Item item;
        QString uuid;
        QString category;
        QString exportCategory;
        QString templateType;
        if (!optionalString(object, "uuid", uuid) || !optionalString(object, "title", item.title) ||
            !optionalString(object, "subtitle", item.subtitle) || !optionalString(object, "note", item.note) ||
            !optionalString(object, "category", category) || !optionalString(object, "category_name", exportCategory) ||
            !optionalString(object, "template_type", templateType) ||
            !optionalFlag(object, "favorite", item.favourite) || !optionalFlag(object, "trashed", item.trashed) ||
            !optionalFlag(object, "archived", item.archived) || !optionalFlag(object, "auto_submit", item.autoSubmit)) {
            return result;
        }
        item.id = QUuid::fromString(uuid);
        if (item.id.isNull()) {
            item.id = QUuid::createUuid();
        }
        item.created = timeOf(object.value(QStringLiteral("createdAt")));
        item.modified = timeOf(object.value(QStringLiteral("updated_at")));
        if (!item.created.isValid()) {
            item.created = QDateTime::currentDateTimeUtc();
        }
        if (!item.modified.isValid()) {
            item.modified = item.created;
        }

        // The category: a built-in one, or one of the person's own, by its
        // own id, named as the export names it.
        if (category.isEmpty()) {
            category = QStringLiteral("misc");
        }
        if (!categoryIds.contains(category)) {
            QString id = builtInCategoryFor(category);
            if (id.isEmpty()) {
                id = category;
                if (!categoryKnown(id)) {
                    result.categories.append(Category{id, exportCategory.isEmpty() ? category : exportCategory});
                }
            }
            categoryIds.insert(category, id);
        }
        item.categoryId = categoryIds.value(category);

        // The fields, in the export's order; equal orders keep their place.
        const QJsonValue fieldList = object.value(QStringLiteral("fields"));
        if (!fieldList.isUndefined() && !fieldList.isNull()) {
            if (!fieldList.isArray() || fieldList.toArray().size() > largestListCount) {
                return result;
            }
            QList<std::pair<qint64, Field>> ordered;
            for (const QJsonValue& fieldValue : fieldList.toArray()) {
                const auto field = fieldFrom(fieldValue);
                if (!field) {
                    return result;
                }
                const QJsonValue order = fieldValue.toObject().value(QStringLiteral("order"));
                ordered.append({isInt(order) ? static_cast<qint64>(order.toDouble()) : 0, *field});
            }
            std::stable_sort(ordered.begin(), ordered.end(),
                             [](const auto& left, const auto& right) { return left.first < right.first; });
            for (const auto& [order, field] : ordered) {
                item.fields.append(field);
            }
        }
        giveFieldsIds(item);

        // The template: a built-in one, or one of the person's own made from
        // this item's fields, empty, as the reference program keeps it.
        if (templateType.isEmpty()) {
            templateType = item.categoryId;
        }
        if (!templateIds.contains(templateType)) {
            QString id = builtInTemplateFor(templateType);
            if (id.isEmpty()) {
                id = templateType;
                if (!templateKnown(id)) {
                    Template made;
                    made.id = id;
                    made.categoryId = item.categoryId;
                    made.name = exportCategory.isEmpty() ? categoryName(item.categoryId, existing) : exportCategory;
                    for (Field field : std::as_const(item.fields)) {
                        if (field.deleted) {
                            continue;
                        }
                        field.value.clear();
                        field.history.clear();
                        field.changed = QDateTime();
                        made.fields.append(field);
                    }
                    result.templates.append(made);
                }
            }
            templateIds.insert(templateType, id);
        }
        item.templateId = templateIds.value(templateType);

        // The icon: a custom one by its uuid; a website's, by its site.
        const QJsonObject icon = object.value(QStringLiteral("icon")).toObject();
        const QString iconUuid = icon.value(QStringLiteral("uuid")).toString();
        if (icons.contains(iconUuid)) {
            item.icon = icons.value(iconUuid);
        }
        item.website = icon.value(QStringLiteral("fav")).toString();

        // Tags.
        const QJsonValue itemFolders = object.value(QStringLiteral("folders"));
        if (itemFolders.isArray()) {
            for (const QJsonValue& folder : itemFolders.toArray()) {
                const QString tag = tagPath(folder.toString());
                if (!tag.isEmpty() && !item.tags.contains(tag)) {
                    item.tags.append(tag);
                }
            }
        }

        // Attachments, base64, each within Chrono Vault's limits (D-029).
        const QJsonValue attachments = object.value(QStringLiteral("attachments"));
        if (!attachments.isUndefined() && !attachments.isNull()) {
            if (!attachments.isArray() || attachments.toArray().size() > largestListCount) {
                return result;
            }
            for (const QJsonValue& value : attachments.toArray()) {
                const QJsonObject attachmentObject = value.toObject();
                QString name;
                QString data;
                QString attachmentUuid;
                if (!value.isObject() || !optionalString(attachmentObject, "name", name) ||
                    !optionalString(attachmentObject, "data", data) ||
                    !optionalString(attachmentObject, "uuid", attachmentUuid) ||
                    data.size() > (largestAttachmentBytes / 3 + 1) * 4) {
                    return result;
                }
                const auto decoded =
                    QByteArray::fromBase64Encoding(data.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
                if (!decoded || decoded.decoded.size() > largestAttachmentBytes) {
                    return result;
                }
                Attachment attachment;
                attachment.id = QUuid::fromString(attachmentUuid);
                if (attachment.id.isNull()) {
                    attachment.id = QUuid::createUuid();
                }
                attachment.name = name.isEmpty() ? QStringLiteral("attachment") : name;
                attachment.data = decoded.decoded;
                item.attachments.append(attachment);
                ++result.attachmentCount;
            }
        }
        result.items.append(item);
    }
    result.status = ImportStatus::Read;
    return result;
}

} // namespace

ImportResult readReferenceExport(const QByteArray& json, const VaultContents& existing)
{
    ImportResult result = readExport(json, existing);
    if (result.status != ImportStatus::Read) {
        // Refused whole: nothing of what was read before the damage (OR-4).
        const ImportStatus status = result.status;
        result = ImportResult{};
        result.status = status;
    }
    return result;
}

} // namespace chrono_vault
