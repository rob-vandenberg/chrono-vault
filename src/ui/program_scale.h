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

#include <QFont>

// --- Version ------------------------------------------------------------
// program_scale.h 0.6.95

// --- Version history ----------------------------------------------------
// v0.6.95: UI scaling to 200%, Options in Tools, Lock vault in File, first sizes and headings (the owner)
// v0.6.92: The font at 100% for the windows (D-069)
// v0.0.11: A mark every 25% (the owner)
// v0.0.10: In steps of 5%; every size follows the factor (D-023)
// v0.0.7: Initial release, as in Chrono Viewer (D-023)

namespace chrono_vault {

// The size of everything Chrono Vault shows, in percent of the size the
// desktop itself uses (D-023): 100% follows the desktop, so a desktop that
// already scales by 150% is not scaled twice.
inline constexpr int smallestProgramScale = 50;
inline constexpr int largestProgramScale = 200;
// The marks above the slider, every 25% (the owner); the slider moves in steps of 5% between them.
inline constexpr int programScaleMarkEvery = 25;
inline constexpr int programScaleToBeginWith = 100;
// The slider moves in steps of 5% (D-023).
inline constexpr int programScaleStep = 5;

// Nothing beyond the two ends, and always on a step.
[[nodiscard]] int programScaleHeldToItsLimits(int percent);

// Puts a scale in use at once, on every window: the program's font is the
// desktop's size times the scale, always worked out from the desktop's own
// size, never from a size already scaled; every other size follows the same
// factor through UiScale.
void applyProgramScale(int percent);
// The font at 100%, whatever the scale: what Chrono Vault's windows draw their
// texts in, enlarged by exactly the scale (D-069). The program's own font is the
// scaled one, for the windows that are not Chrono Vault's (the file dialogs).
[[nodiscard]] QFont programFont();
[[nodiscard]] int programScaleInUse();

// Tests only: the font the scale starts from is taken again from the program.
void forgetDesktopFontForTests();

} // namespace chrono_vault
