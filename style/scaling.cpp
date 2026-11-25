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
#include "scaling.h"

#include <QQmlEngine>

#include <algorithm>
#include <cmath>

// --- Version ------------------------------------------------------------
// scaling.cpp 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Enlarged by exactly the UI scaling (D-069)
// v0.0.10: Initial release (D-023)

namespace chrono_vault {

UiScale* UiScale::instance()
{
    static UiScale* const one = new UiScale();
    return one;
}

UiScale* UiScale::create(QQmlEngine* engine, QJSEngine* script)
{
    Q_UNUSED(engine);
    Q_UNUSED(script);
    UiScale* one = instance();
    // Owned by the program, never by an engine that ends before it.
    QQmlEngine::setObjectOwnership(one, QQmlEngine::CppOwnership);
    return one;
}

void UiScale::setFactor(double factor)
{
    if (factor > 0 && factor != currentFactor) {
        currentFactor = factor;
        emit factorChanged();
    }
}

double UiScale::whole(double pixels)
{
    const double factor = instance()->currentFactor;
    return std::ceil(pixels / factor - 0.001) * factor;
}

int UiScale::px(double pixels) const
{
    return std::max(1, static_cast<int>(std::lround(pixels * currentFactor)));
}

} // namespace chrono_vault
