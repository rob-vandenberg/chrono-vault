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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// --- Version ------------------------------------------------------------
// vault_contents.cpp 0.0.2

// --- Version history ----------------------------------------------------
// v0.0.2: Initial release

namespace chrono_vault {

namespace {

constexpr int contentsFormat = 1;
constexpr qsizetype largestItemCount = 100000;
constexpr qsizetype largestFieldCount = 1000;

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

QString templateName(ItemTemplate itemTemplate)
{
    return itemTemplate == ItemTemplate::SecureNote ? QStringLiteral("note") : QStringLiteral("login");
}

std::optional<ItemTemplate> templateFromName(const QString& name)
{
    if (name == QLatin1String("login")) {
        return ItemTemplate::Login;
    }
    if (name == QLatin1String("note")) {
        return ItemTemplate::SecureNote;
    }
    return std::nullopt;
}

bool isString(const QJsonObject& object, const char* key)
{
    return object.value(QLatin1String(key)).isString();
}

} // namespace

Item newItem(ItemTemplate itemTemplate)
{
    Item item;
    item.id = QUuid::createUuid();
    item.itemTemplate = itemTemplate;
    item.created = QDateTime::currentDateTimeUtc();
    item.modified = item.created;
    if (itemTemplate == ItemTemplate::Login) {
        item.fields = {
            {QStringLiteral("username"), {}, FieldKind::Text, {}},
            {QStringLiteral("password"), {}, FieldKind::Password, {}},
            {QStringLiteral("url"), {}, FieldKind::WebAddress, {}},
            {QStringLiteral("totp"), {}, FieldKind::OneTimeCode, {}},
            {QStringLiteral("notes"), {}, FieldKind::MultiLine, {}},
        };
    } else {
        item.fields = {{QStringLiteral("notes"), {}, FieldKind::MultiLine, {}}};
    }
    return item;
}

QString roleName(const QString& role)
{
    if (role == QLatin1String("username")) {
        return QCoreApplication::translate("Field", "User name");
    }
    if (role == QLatin1String("password")) {
        return QCoreApplication::translate("Field", "Password");
    }
    if (role == QLatin1String("url")) {
        return QCoreApplication::translate("Field", "Web address");
    }
    if (role == QLatin1String("totp")) {
        return QCoreApplication::translate("Field", "One-time code");
    }
    if (role == QLatin1String("notes")) {
        return QCoreApplication::translate("Field", "Notes");
    }
    return role;
}

QByteArray contentsToJson(const VaultContents& contents)
{
    QJsonArray items;
    for (const Item& item : contents.items) {
        QJsonArray fields;
        for (const Field& field : item.fields) {
            QJsonObject object{{QStringLiteral("kind"), kindName(field.kind)},
                               {QStringLiteral("value"), field.value}};
            if (field.isCustom()) {
                object.insert(QStringLiteral("label"), field.label);
            } else {
                object.insert(QStringLiteral("role"), field.role);
            }
            fields.append(object);
        }
        items.append(QJsonObject{
            {QStringLiteral("id"), item.id.toString(QUuid::WithoutBraces)},
            {QStringLiteral("template"), templateName(item.itemTemplate)},
            {QStringLiteral("title"), item.title},
            {QStringLiteral("created"), item.created.toString(Qt::ISODateWithMs)},
            {QStringLiteral("modified"), item.modified.toString(Qt::ISODateWithMs)},
            {QStringLiteral("fields"), fields},
        });
    }
    const QJsonObject root{{QStringLiteral("format"), contentsFormat}, {QStringLiteral("items"), items}};
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
    const QJsonValue format = root.value(QStringLiteral("format"));
    if (!format.isDouble()) {
        return result;
    }
    if (format.toInt() > contentsFormat) {
        result.status = ContentsStatus::NewerFormat;
        return result;
    }
    if (format.toInt() != contentsFormat || !root.value(QStringLiteral("items")).isArray()) {
        return result;
    }
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    if (items.size() > largestItemCount) {
        return result;
    }

    VaultContents contents;
    contents.items.reserve(items.size());
    for (const QJsonValue& itemValue : items) {
        if (!itemValue.isObject()) {
            return result;
        }
        const QJsonObject object = itemValue.toObject();
        if (!isString(object, "id") || !isString(object, "template") || !isString(object, "title") ||
            !isString(object, "created") || !isString(object, "modified") ||
            !object.value(QStringLiteral("fields")).isArray()) {
            return result;
        }
        Item item;
        item.id = QUuid::fromString(object.value(QStringLiteral("id")).toString());
        const auto itemTemplate = templateFromName(object.value(QStringLiteral("template")).toString());
        if (item.id.isNull() || !itemTemplate) {
            return result;
        }
        item.itemTemplate = *itemTemplate;
        item.title = object.value(QStringLiteral("title")).toString();
        item.created = QDateTime::fromString(object.value(QStringLiteral("created")).toString(), Qt::ISODateWithMs);
        item.modified = QDateTime::fromString(object.value(QStringLiteral("modified")).toString(), Qt::ISODateWithMs);

        const QJsonArray fields = object.value(QStringLiteral("fields")).toArray();
        if (fields.size() > largestFieldCount) {
            return result;
        }
        for (const QJsonValue& fieldValue : fields) {
            if (!fieldValue.isObject()) {
                return result;
            }
            const QJsonObject fieldObject = fieldValue.toObject();
            const auto kind = kindFromName(fieldObject.value(QStringLiteral("kind")).toString());
            const bool hasRole = isString(fieldObject, "role");
            const bool hasLabel = isString(fieldObject, "label");
            if (!kind || !isString(fieldObject, "value") || hasRole == hasLabel) {
                return result;
            }
            Field field;
            field.kind = *kind;
            field.value = fieldObject.value(QStringLiteral("value")).toString();
            if (hasRole) {
                field.role = fieldObject.value(QStringLiteral("role")).toString();
                if (field.role.isEmpty()) {
                    return result;
                }
            } else {
                field.label = fieldObject.value(QStringLiteral("label")).toString();
            }
            item.fields.append(field);
        }
        contents.items.append(item);
    }
    result.status = ContentsStatus::Read;
    result.contents = contents;
    return result;
}

} // namespace chrono_vault
