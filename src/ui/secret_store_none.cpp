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


#include "secret_store.h"

// --- Version ------------------------------------------------------------
// secret_store_none.cpp 0.3.60

// --- Version history ----------------------------------------------------
// v0.3.60: Initial release (D-057)

// No system store here yet (macOS, Android and iOS come later, D-009): the
// WebDAV password is then asked once per unlock and kept in memory only.

namespace chrono_vault {

namespace {

class NoStore : public SecretStore
{
public:
    bool keep(const QString&, const QByteArray&) override { return false; }
    std::pair<Result, QByteArray> find(const QString&) override { return {Result::Unavailable, {}}; }
    bool remove(const QString&) override { return true; }
};

} // namespace

std::unique_ptr<SecretStore> makeSecretStore()
{
    return std::make_unique<NoStore>();
}

} // namespace chrono_vault
