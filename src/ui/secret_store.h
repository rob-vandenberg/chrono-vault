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

#include <memory>
#include <utility>

// --- Version ------------------------------------------------------------
// secret_store.h 0.3.60

// --- Version history ----------------------------------------------------
// v0.3.60: Initial release (D-057)

namespace chrono_vault {

// The system's own store for secrets (D-057): the desktop keyring through
// the Secret Service on Linux, the Credential Manager on Windows. The
// WebDAV password is kept there, on this device only, never in a settings
// file. Every call may be slow or ask the person: never on the window's
// thread (OR-5).
class SecretStore
{
public:
    enum class Result { Done, NoSecret, Unavailable };

    virtual ~SecretStore() = default;
    // False when it could not be kept.
    virtual bool keep(const QString& name, const QByteArray& secret) = 0;
    virtual std::pair<Result, QByteArray> find(const QString& name) = 0;
    // True when nothing under the name is left.
    virtual bool remove(const QString& name) = 0;
};

// This system's store; one that is always Unavailable where there is none.
[[nodiscard]] std::unique_ptr<SecretStore> makeSecretStore();

} // namespace chrono_vault
