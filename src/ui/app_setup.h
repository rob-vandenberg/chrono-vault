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

#include <QIcon>

class QWindow;

// --- Version ------------------------------------------------------------
// app_setup.h 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Initial release

namespace chrono_vault {

// Chrono Vault's own look, the same on every platform (D-017): Qt's Fusion
// style and the Noto fonts carried inside the program, Noto Sans for all
// text. Called before any window loads, by the program and by the tests
// alike, so the tests check what people see.
void applyStyle();

// The monospaced font, for passwords and one-time codes.
inline constexpr char monospacedFontFamily[] = "Noto Sans Mono";

// Shows the main window as the program starts: hidden in the tray when
// "Start Chrono Vault minimized" is on and there is a tray, minimized when
// there is none, and normally otherwise (D-019).
void showMainWindow(QWindow* window, bool startMinimized, bool trayAvailable);

// --reset on the command line: the settings are not read from the settings
// file at the start, so they start at their defaults (D-026).
void setSkipReadingSettings(bool skip);
[[nodiscard]] bool skipReadingSettings();

// The settings from before 0.0.30, kept under the name "Chrono Vault", are
// carried over to "chrono-vault" (the identifier) once, nothing lost: the old
// place is removed only after every value reads back the same (D-044).
// Returns false when something did not read back; the old place then stays.
bool carryOverOldSettings(const QString& oldOrganization, const QString& oldApplication);

// The program's own icon, art/chrono-vault.svg (D-019).
[[nodiscard]] QIcon programIcon();

} // namespace chrono_vault
