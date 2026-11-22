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

// --- Version ------------------------------------------------------------
// json_export.h 0.0.22

// --- Version history ----------------------------------------------------
// v0.0.22: Initial release (D-037)

namespace chrono_vault {

// The vault's contents in the reference program's JSON format, exactly as
// the importer reads it (D-037): every item, the Trash and the Archive
// included, every field with its type, label, value, hide mark, order, id
// and history, sections, hidden and deleted fields, attachments, custom and
// website icons, and tags as nested folders. Unencrypted: the caller says so.
[[nodiscard]] QByteArray writeReferenceExport(const VaultContents& contents);

} // namespace chrono_vault
