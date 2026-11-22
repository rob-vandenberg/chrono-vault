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

#include <QObject>

#include <memory>

// --- Version ------------------------------------------------------------
// system_activity.h 0.0.25

// --- Version history ----------------------------------------------------
// v0.0.25: Initial release (D-040)

namespace chrono_vault {

// What the system says about the person's computer, for the autolock
// (D-040): that it goes to sleep (or wakes from it), and how long the whole
// system has had no keyboard or mouse input.
class SystemActivity : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Asks how long the system has been inactive; answered by idleTime,
    // never waiting on the window's thread.
    virtual void askIdleTime() = 0;

signals:
    // The computer goes to sleep, or wakes from a sleep that was not seen
    // going in.
    void sleeping();
    // Seconds without input anywhere on the computer; -1 when the system
    // does not tell.
    void idleTime(qint64 seconds);
};

// The one for this system.
[[nodiscard]] std::unique_ptr<SystemActivity> makeSystemActivity();

} // namespace chrono_vault
