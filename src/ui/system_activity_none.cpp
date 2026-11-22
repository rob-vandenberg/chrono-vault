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
#include "system_activity.h"

#include <QTimer>

// --- Version ------------------------------------------------------------
// system_activity_none.cpp 0.0.25

// --- Version history ----------------------------------------------------
// v0.0.25: Initial release (D-040)

// Other systems (macOS for now): neither sleep nor inactivity is told yet.

namespace chrono_vault {

namespace {

class NoSystemActivity : public SystemActivity
{
public:
    void askIdleTime() override
    {
        QTimer::singleShot(0, this, [this]() { emit idleTime(-1); });
    }
};

} // namespace

std::unique_ptr<SystemActivity> makeSystemActivity()
{
    return std::make_unique<NoSystemActivity>();
}

} // namespace chrono_vault
