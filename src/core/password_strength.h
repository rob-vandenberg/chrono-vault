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
#include <QStringList>

// --- Version ------------------------------------------------------------
// password_strength.h 0.6.86

// --- Version history ----------------------------------------------------
// v0.6.86: Initial release (D-065)

namespace chrono_vault {

// How hard a password is to guess, in bits: the cheapest way to make it from
// characters drawn at random, common passwords, words, dates, sequences,
// keyboard rows, repeats and what the item itself says (its title, its user
// name, its website). Our own estimate, no word list of others (D-065).
[[nodiscard]] double passwordBits(const QString& password, const QStringList& personal = {});

// Under this a password is weak.
inline constexpr double weakBelowBits = 50.0;

} // namespace chrono_vault
