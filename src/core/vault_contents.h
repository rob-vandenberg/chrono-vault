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
#include <QUuid>

#include <optional>

// --- Version ------------------------------------------------------------
// vault_contents.h 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: An item's own icon, and the contents format 2
// v0.0.2: Initial release

// What a vault holds (D-015): items, each made from a template, each a list
// of fields. A built-in field is known by its role, so its name is shown in
// the person's own language; a custom field carries the name they gave it.
namespace chrono_vault {

enum class FieldKind { Text, Password, WebAddress, OneTimeCode, MultiLine };

enum class ItemTemplate { Login, SecureNote };

struct Field
{
    QString role;  // "username", "password", "url", "totp", "notes"; empty for a custom field
    QString label; // the name of a custom field
    FieldKind kind = FieldKind::Text;
    QString value;

    [[nodiscard]] bool isCustom() const { return role.isEmpty(); }
    friend bool operator==(const Field&, const Field&) = default;
};

struct Item
{
    QUuid id;
    ItemTemplate itemTemplate = ItemTemplate::Login;
    QString title;
    QList<Field> fields;
    QByteArray icon; // PNG, 64 by 64 at most; empty for the template's own icon (D-018)
    QDateTime created;
    QDateTime modified;

    friend bool operator==(const Item&, const Item&) = default;
};

struct VaultContents
{
    QList<Item> items;

    friend bool operator==(const VaultContents&, const VaultContents&) = default;
};

enum class ContentsStatus { Read, NewerFormat, Damaged };

struct ContentsResult
{
    ContentsStatus status = ContentsStatus::Damaged;
    VaultContents contents;
};

// A new, empty item of a template, with the template's fields.
[[nodiscard]] Item newItem(ItemTemplate itemTemplate);

// The name of a built-in field in the person's language.
[[nodiscard]] QString roleName(const QString& role);

[[nodiscard]] QByteArray contentsToJson(const VaultContents& contents);

// Reads contents written by contentsToJson. Anything of the wrong shape makes
// the whole read fail rather than lose a part of it silently (OR-15); a
// newer format is refused so it is never written back without what it holds.
[[nodiscard]] ContentsResult contentsFromJson(const QByteArray& json);

} // namespace chrono_vault
