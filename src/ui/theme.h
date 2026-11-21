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

#include <QPalette>
#include <QString>

// --- Version ------------------------------------------------------------
// theme.h 0.0.7

// --- Version history ----------------------------------------------------
// v0.0.7: Initial release, as in Chrono Viewer (D-023)

namespace chrono_vault {

// The person chooses the look; the program never chooses for them (D-023).
// The numbers are what the settings window and the settings file use.
enum class Theme { Light = 0, Dark = 1, FollowTheSystem = 2 };

// The words a theme is kept under in the settings; never translated.
[[nodiscard]] QString nameOfTheme(Theme theme);
[[nodiscard]] Theme themeFromName(const QString& name);

// Which of the two looks a choice comes to now. Follow the system asks the
// system; one that does not say means light.
[[nodiscard]] bool lookIsDark(Theme chosen);

// The program's two palettes, for Qt's Fusion style: every colour Chrono
// Vault shows comes from here (D-023), the same as in Chrono Viewer.
[[nodiscard]] QPalette lightPalette();
[[nodiscard]] QPalette darkPalette();

// Puts a theme in use at once, on every window. With Follow the system it
// also follows the system when that switches between light and dark.
void applyTheme(Theme chosen);
[[nodiscard]] Theme themeInUse();

} // namespace chrono_vault
