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

#include "vault_contents.h"

#include <QList>
#include <QString>

// --- Version ------------------------------------------------------------
// templates.h 0.2.56

// --- Version history ----------------------------------------------------
// v0.2.56: A category's name and icon as the person gave them (D-056)
// v0.0.20: The website an item belongs to, for its icon (D-036)
// v0.0.14: Initial release (D-027)

namespace chrono_vault {

// The built-in categories, in the order the left column shows them (D-027).
[[nodiscard]] QStringList builtInCategoryIds();
[[nodiscard]] bool isBuiltInCategory(const QString& id);

// The built-in templates, each with its category and fields (D-027).
[[nodiscard]] const QList<Template>& builtInTemplates();

// The name a person sees: translated for a built-in one unless the person
// renamed it (D-056), as given for one of the person's own, the id itself
// when it is unknown.
[[nodiscard]] QString categoryName(const QString& id, const VaultContents& contents);
[[nodiscard]] QString templateName(const Template& itemTemplate);

// A built-in template, or one of the person's own; nothing when it is gone.
[[nodiscard]] std::optional<Template> findTemplate(const QString& id, const VaultContents& contents);

// A new item from a template: a new id, the template's fields, empty.
[[nodiscard]] Item newItem(const Template& itemTemplate);

// The website an item belongs to, as a host such as "github.com", for its
// website icon (D-036): the site the import named, else its first web
// address; empty when it has none.
[[nodiscard]] QString websiteHostOf(const Item& item);

// The picture shown for an item without an icon of its own: its category's,
// the icon the person gave it if any (D-056), else the program's own.
[[nodiscard]] QString categoryIconSource(const QString& categoryId, const VaultContents& contents);

// What the person changed of a built-in category; nothing when unchanged.
[[nodiscard]] const Category* builtInCategoryChange(const QString& id, const VaultContents& contents);

} // namespace chrono_vault
