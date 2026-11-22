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
// vault_contents.h 0.0.14

// --- Version history ----------------------------------------------------
// v0.0.14: Categories and templates of the person's own, tags, favourites and attachments; the contents format 3
//          (D-027, D-028, D-029)
// v0.0.4: An item's own icon
// v0.0.2: Initial release

namespace chrono_vault {

enum class FieldKind { Text, Password, WebAddress, OneTimeCode, MultiLine };

struct Field
{
    QString role;  // a built-in field's role, such as "username"; empty for a custom field
    QString label; // the name of a custom field
    FieldKind kind = FieldKind::Text;
    QString value;

    [[nodiscard]] bool isCustom() const { return role.isEmpty(); }
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

struct Item
{
    QUuid id;
    QString templateId = QStringLiteral("login"); // the template it was made from (D-027)
    QString categoryId = QStringLiteral("login");
    QString title;
    QList<Field> fields;
    QStringList tags; // each tag nested with "/", such as "Work/Servers" (D-028)
    bool favourite = false;
    QList<Attachment> attachments;
    QByteArray icon; // PNG, 64 by 64 at most; empty for the category's own icon (D-018)
    QDateTime created;
    QDateTime modified;

    friend bool operator==(const Item&, const Item&) = default;
};

// A category the person made (D-027). The built-in ones are in templates.h.
struct Category
{
    QString id;
    QString name;

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

// A tag as it is kept: every part trimmed, empty parts left out, joined by
// "/". Empty when nothing is left.
[[nodiscard]] QString normalizedTag(const QString& tag);

[[nodiscard]] QByteArray contentsToJson(const VaultContents& contents);

[[nodiscard]] ContentsResult contentsFromJson(const QByteArray& json);

} // namespace chrono_vault
