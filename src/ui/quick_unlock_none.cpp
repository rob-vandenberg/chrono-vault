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
#include "quick_unlock.h"

// --- Version ------------------------------------------------------------
// quick_unlock_none.cpp 0.0.23

// --- Version history ----------------------------------------------------
// v0.0.23: Initial release (D-038)

// Where there is no fingerprint unlock yet (macOS, until it is reached).

namespace chrono_vault {

namespace {

class NoQuickUnlock : public QuickUnlock
{
public:
    [[nodiscard]] bool available() const override { return false; }
    [[nodiscard]] QString name() const override { return {}; }
    void store(const QString&, const QByteArray&) override { emit stored(false); }
    void retrieve(const QString&) override { emit retrieved(Result::Unavailable, {}); }
    void remove(const QString& vault) override { remember(vault, false); }
};

} // namespace

std::unique_ptr<QuickUnlock> makeQuickUnlock()
{
    return std::make_unique<NoQuickUnlock>();
}

} // namespace chrono_vault
