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

// --- Version ------------------------------------------------------------
// program_scale.h 0.0.11

// --- Version history ----------------------------------------------------
// v0.0.11: A mark every 25% (the owner)
// v0.0.10: In steps of 5%; every size follows the factor (D-023)
// v0.0.7: Initial release, as in Chrono Viewer (D-023)

namespace chrono_vault {

// The size of everything Chrono Vault shows, in percent of the size the
// desktop itself uses (D-023): 100% follows the desktop, so a desktop that
// already scales by 150% is not scaled twice.
inline constexpr int smallestProgramScale = 50;
inline constexpr int largestProgramScale = 400;
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
[[nodiscard]] int programScaleInUse();

// Tests only: the font the scale starts from is taken again from the program.
void forgetDesktopFontForTests();

} // namespace chrono_vault
