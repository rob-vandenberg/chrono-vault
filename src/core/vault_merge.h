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

#include <QDateTime>

// --- Version ------------------------------------------------------------
// vault_merge.h 0.2.45

// --- Version history ----------------------------------------------------
// v0.2.45: Initial release (D-054)

namespace chrono_vault {

struct MergeResult
{
    VaultContents contents;
    int itemsChangedOnBoth = 0; // items with a value changed differently on both devices
    int itemsBack = 0;          // items deleted on one device and changed on the other, kept
};

// Merges what this device holds (mine) with the version found on the other
// side (theirs), against the last version both agreed on (base), which may
// be missing (D-054). A change made on one side only is taken; a value
// changed differently on both is settled by the newest change, and the
// other value is kept in the field's history and in a note on the item; an
// item deleted on one side and changed on the other is kept, with a note.
// Nothing is dropped silently (OR-15).
[[nodiscard]] MergeResult mergeContents(const VaultContents* base, const VaultContents& mine,
                                        const VaultContents& theirs, const QDateTime& now);

} // namespace chrono_vault
