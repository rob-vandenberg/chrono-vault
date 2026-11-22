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

#include <QByteArray>
#include <QString>

// --- Version ------------------------------------------------------------
// json_import.h 0.0.19

// --- Version history ----------------------------------------------------
// v0.0.19: Initial release (D-035)

namespace chrono_vault {

enum class ImportStatus { Read, NotTheFormat, Damaged, TooLarge };

// What an export holds, made into Chrono Vault's items, categories and
// templates, ready to be added to a vault (D-035).
struct ImportResult
{
    ImportStatus status = ImportStatus::Damaged;
    QList<Item> items;
    QList<Category> categories; // the categories of the person's own it needs
    QList<Template> templates;  // the templates of the person's own it needs
    int attachmentCount = 0;
};

// The largest export that is read at all (OR-4).
inline constexpr qsizetype largestImportBytes = 300LL * 1024 * 1024;

// Reads the reference program's JSON export, exactly as it holds it (D-035):
// every item, field, section, hidden and deleted field, field history,
// attachment and custom icon. An export of the wrong shape is refused whole,
// never half-read (OR-4). Categories and templates already in the vault are
// not made again.
[[nodiscard]] ImportResult readReferenceExport(const QByteArray& json, const VaultContents& existing);

// The name of the program whose export this importer reads, for its window.
[[nodiscard]] QString referenceProgramName();

} // namespace chrono_vault
