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

#include <QByteArray>
#include <QString>

#include <optional>

// --- Version ------------------------------------------------------------
// item_icons.h 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Initial release

// An item's own icon, picked by the person (D-018): JPG, PNG, BMP, ICO or
// SVG, at most 5 MB, scaled to fit 64 by 64 and kept as PNG.
namespace chrono_vault {

inline constexpr qint64 largestIconFileBytes = 5LL * 1024 * 1024;
inline constexpr int iconSide = 64;

enum class IconStatus { Read, NotFound, TooLarge, NotAPicture, NotSupported };

struct IconResult
{
    IconStatus status = IconStatus::NotAPicture;
    QByteArray png;
};

[[nodiscard]] IconResult readIcon(const QString& path);

// What to tell the person when a picked file is not used.
[[nodiscard]] QString iconStatusText(IconStatus status);

} // namespace chrono_vault
