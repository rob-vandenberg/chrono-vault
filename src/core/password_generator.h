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
// password_generator.h 0.0.2

// --- Version history ----------------------------------------------------
// v0.0.2: Initial release

namespace chrono_vault {

struct GeneratorOptions
{
    int length = 20;
    bool uppercase = true;
    bool lowercase = true;
    bool digits = true;
    bool symbols = true;
};

inline constexpr int shortestGeneratedPassword = 4;
inline constexpr int longestGeneratedPassword = 128;

// A password from the system's secure random source, with at least one
// character of every chosen kind. A length outside 4 to 128 is brought within
// it; with no kind chosen, lowercase letters are used. Empty only when the
// random source fails.
[[nodiscard]] QString generatePassword(const GeneratorOptions& options);

} // namespace chrono_vault
