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

#include <QString>

// --- Version ------------------------------------------------------------
// autostart.h 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Initial release

// "Start Chrono Vault at startup" (D-019): on Linux the standard autostart
// entry in the person's own configuration folder, on Windows the person's
// own "Run" entry in the registry. Neither needs administrator rights.
namespace chrono_vault::autostart {

[[nodiscard]] bool isEnabled();

// Writes or removes the entry that starts this program at login; false
// when it could not be written.
bool setEnabled(bool enabled, const QString& program);

// Tests only: the registry key used on Windows instead of the real one.
void setWindowsKeyForTests(const QString& key);

} // namespace chrono_vault::autostart
