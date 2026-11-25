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

#include "scaling.h"

#include <QFont>
#include <QGuiApplication>

#include <algorithm>
#include <cmath>
#include <optional>

// --- Version ------------------------------------------------------------
// program_scale.cpp 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: The font at 100% for the windows (D-069)
// v0.0.10: In steps of 5%; every size follows the factor, not only the text (D-023)
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
    // In steps of 5% (D-023).
    const int stepped = static_cast<int>(std::lround(percent / static_cast<double>(programScaleStep))) * programScaleStep;
    return std::clamp(stepped, smallestProgramScale, largestProgramScale);
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
    // Everything else follows the same factor: icons, tick boxes, spacing,
    // fixed sizes (D-023).
    UiScale::instance()->setFactor(held / 100.0);
    if (ours != QGuiApplication::font()) {
        QGuiApplication::setFont(ours);
    }
}

QFont programFont()
{
    return desktopFont() ? *desktopFont() : QGuiApplication::font();
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
