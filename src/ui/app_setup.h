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
// app_setup.h 0.2.54

// --- Version history ----------------------------------------------------
// v0.2.54: The font size of our own, 11 points (the owner, D-017)
// v0.0.38: --reset and the carry-over moved out (D-026)
// v0.0.4: Initial release

namespace chrono_vault {

// Chrono Vault's own look, the same on every platform (D-017): Qt's Fusion
// style and the Noto fonts carried inside the program, Noto Sans for all
// text. Called before any window loads, by the program and by the tests
// alike, so the tests check what people see.
void applyStyle();

// The size of every text before the scalings, the same on every system
// (the owner, D-017).
inline constexpr double programFontPoints = 11.0;

// The monospaced font, for passwords and one-time codes.
inline constexpr char monospacedFontFamily[] = "Noto Sans Mono";

// Shows the main window as the program starts: hidden in the tray when
// "Start Chrono Vault minimized" is on and there is a tray, minimized when
// there is none, and normally otherwise (D-019).
void showMainWindow(QWindow* window, bool startMinimized, bool trayAvailable);

// The program's own icon, art/chrono-vault.svg (D-019).
[[nodiscard]] QIcon programIcon();

} // namespace chrono_vault
