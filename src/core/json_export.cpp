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
#include "json_export.h"

#include "templates.h"

#include <QCryptographicHash>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>

// --- Version ------------------------------------------------------------
// json_export.cpp 0.0.22

// --- Version history ----------------------------------------------------
// v0.0.22: Initial release (D-037)

namespace chrono_vault {

namespace {

// Chrono Vault's built-in categories as the export names them; the
// person's own keep their own id, as the importer gave it.
QString exportCategory(const QString& id)
{
    static const QHash<QString, QString> names{{QStringLiteral("card"), QStringLiteral("creditcard")}};
    return names.value(id, id);
}

// Chrono Vault's built-in templates as the export names them.
QString exportTemplate(const QString& id)
{
    static const QHash<QString, QString> names{
        {QStringLiteral("login"), QStringLiteral("login.default")},
        {QStringLiteral("note"), QStringLiteral("note.default")},
        {QStringLiteral("password"), QStringLiteral("password.default")},
        {QStringLiteral("card"), QStringLiteral("creditcard.default")},
        {QStringLiteral("identity"), QStringLiteral("identity.default")},
    };
    return names.value(id, id);
}

QString exportType(const Field& field)
{
    if (field.kind == FieldKind::Hidden && !field.hiddenType.isEmpty()) {
        return field.hiddenType;
    }
    return fieldKindName(field.kind);
}

qint64 secondsOf(const QDateTime& moment)
{
    return moment.isValid() ? moment.toSecsSinceEpoch() : 0;
}

// A tag's folder id: the same for the same path, in every export.
QString folderId(const QString& path)
{
    return QUuid::createUuidV5(QUuid(), path.toUtf8()).toString(QUuid::WithoutBraces);
}

} // namespace

QByteArray writeReferenceExport(const VaultContents& contents)
{
    QJsonArray icons;
    QHash<QByteArray, QString> iconIds; // the same picture once
    QJsonArray folders;
    QStringList foldersMade;
    QJsonArray items;
    const QMimeDatabase mimeTypes;

    for (const Item& item : contents.items) {
        QJsonArray fields;
        int order = 0;
        for (const Field& field : item.fields) {
            QJsonArray history;
            for (const FieldChange& change : field.history) {
                history.append(QJsonObject{{QStringLiteral("value"), change.value},
                                           {QStringLiteral("updated_at"), secondsOf(change.changed)},
                                           {QStringLiteral("encrypted"), false}});
            }
            QJsonObject object{
                {QStringLiteral("type"), exportType(field)},
                {QStringLiteral("label"), field.isCustom() ? field.label : roleName(field.role)},
                {QStringLiteral("value"), field.value},
                {QStringLiteral("sensitive"), field.isHidden() ? 1 : 0},
                {QStringLiteral("deleted"), field.deleted ? 1 : 0},
                {QStringLiteral("order"), ++order},
                {QStringLiteral("uid"), static_cast<double>(field.uid)},
                {QStringLiteral("updated_at"), secondsOf(field.changed)},
                {QStringLiteral("value_updated_at"), secondsOf(field.changed)},
            };
            if (!history.isEmpty()) {
                object.insert(QStringLiteral("history"), history);
            }
            fields.append(object);
        }

        // The icon: a custom one by its uuid; a website's by its site.
        QJsonObject icon{{QStringLiteral("image"), QJsonObject{{QStringLiteral("file"),
                                                                QStringLiteral("misc/") + exportCategory(item.categoryId)}}}};
        if (!item.icon.isEmpty()) {
            QString id = iconIds.value(item.icon);
            if (id.isEmpty()) {
                id = QUuid::createUuidV5(QUuid(), QCryptographicHash::hash(item.icon, QCryptographicHash::Sha256))
                         .toString(QUuid::WithoutBraces);
                iconIds.insert(item.icon, id);
                icons.append(QJsonObject{{QStringLiteral("uuid"), id},
                                         {QStringLiteral("data"), QString::fromLatin1(item.icon.toHex())}});
            }
            icon.insert(QStringLiteral("type"), 2);
            icon.insert(QStringLiteral("uuid"), id);
            icon.insert(QStringLiteral("fav"), QString());
        } else {
            icon.insert(QStringLiteral("type"), 1);
            icon.insert(QStringLiteral("uuid"), QString());
            icon.insert(QStringLiteral("fav"), item.website);
        }

        // Tags as nested folders.
        QJsonArray itemFolders;
        for (const QString& tag : item.tags) {
            const QStringList parts = tag.split(QLatin1Char('/'));
            for (qsizetype depth = 1; depth <= parts.size(); ++depth) {
                const QString path = parts.mid(0, depth).join(QLatin1Char('/'));
                if (foldersMade.contains(path)) {
                    continue;
                }
                foldersMade.append(path);
                const QString parent = depth > 1 ? folderId(parts.mid(0, depth - 1).join(QLatin1Char('/'))) : QString();
                folders.append(QJsonObject{{QStringLiteral("uuid"), folderId(path)},
                                           {QStringLiteral("title"), parts.at(depth - 1)},
                                           {QStringLiteral("parent_uuid"), parent}});
            }
            itemFolders.append(folderId(tag));
        }

        QJsonArray attachments;
        int attachmentOrder = 0;
        for (const Attachment& attachment : item.attachments) {
            attachments.append(QJsonObject{
                {QStringLiteral("uuid"), attachment.id.toString(QUuid::WithoutBraces)},
                {QStringLiteral("name"), attachment.name},
                {QStringLiteral("kind"), mimeTypes.mimeTypeForFileNameAndData(attachment.name, attachment.data).name()},
                {QStringLiteral("data"), QString::fromLatin1(attachment.data.toBase64())},
                {QStringLiteral("order"), ++attachmentOrder},
                {QStringLiteral("updated"), secondsOf(item.modified)},
            });
        }

        QJsonObject object{
            {QStringLiteral("uuid"), item.id.toString(QUuid::WithoutBraces)},
            {QStringLiteral("title"), item.title},
            {QStringLiteral("subtitle"), item.subtitle},
            {QStringLiteral("note"), item.note},
            {QStringLiteral("category"), exportCategory(item.categoryId)},
            {QStringLiteral("category_name"), categoryName(item.categoryId, contents)},
            {QStringLiteral("template_type"), exportTemplate(item.templateId)},
            {QStringLiteral("favorite"), item.favourite ? 1 : 0},
            {QStringLiteral("trashed"), item.trashed ? 1 : 0},
            {QStringLiteral("archived"), item.archived ? 1 : 0},
            {QStringLiteral("auto_submit"), item.autoSubmit ? 1 : 0},
            {QStringLiteral("createdAt"), secondsOf(item.created)},
            {QStringLiteral("updated_at"), secondsOf(item.modified)},
            {QStringLiteral("field_updated_at"), secondsOf(item.modified)},
            {QStringLiteral("icon"), icon},
            {QStringLiteral("fields"), fields},
        };
        if (!itemFolders.isEmpty()) {
            object.insert(QStringLiteral("folders"), itemFolders);
        }
        if (!attachments.isEmpty()) {
            object.insert(QStringLiteral("attachments"), attachments);
        }
        items.append(object);
    }
    QJsonObject root{{QStringLiteral("items"), items}};
    if (!icons.isEmpty()) {
        root.insert(QStringLiteral("custom_icons"), icons);
    }
    if (!folders.isEmpty()) {
        root.insert(QStringLiteral("folders"), folders);
    }
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace chrono_vault
