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
#include "program_scale.h"

#include <QFont>
#include <QGuiApplication>

#include <algorithm>
#include <optional>

// --- Version ------------------------------------------------------------
// program_scale.cpp 0.0.7

// --- Version history ----------------------------------------------------
// v0.0.7: Initial release, as in Chrono Viewer (D-023)

namespace chrono_vault {

namespace {

std::optional<QFont>& desktopFont()
{
    static std::optional<QFont> font;
    return font;
}

int& scaleInUse()
{
    static int percent = programScaleToBeginWith;
    return percent;
}

} // namespace

int programScaleHeldToItsLimits(int percent)
{
    return std::clamp(percent, smallestProgramScale, largestProgramScale);
}

void applyProgramScale(int percent)
{
    const int held = programScaleHeldToItsLimits(percent);
    if (!desktopFont()) {
        desktopFont() = QGuiApplication::font();
    }
    const QFont& desktop = *desktopFont();
    QFont ours = desktop;
    // A font is sized in points or in pixels, as the desktop has it; both are
    // scaled the same way.
    if (desktop.pointSizeF() > 0) {
        ours.setPointSizeF(desktop.pointSizeF() * held / 100.0);
    } else if (desktop.pixelSize() > 0) {
        ours.setPixelSize(std::max(1, static_cast<int>(desktop.pixelSize() * held / 100.0 + 0.5)));
    }
    scaleInUse() = held;
    if (ours != QGuiApplication::font()) {
        QGuiApplication::setFont(ours);
    }
}

int programScaleInUse()
{
    return scaleInUse();
}

void forgetDesktopFontForTests()
{
    desktopFont().reset();
    scaleInUse() = programScaleToBeginWith;
}

} // namespace chrono_vault
