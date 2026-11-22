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

#include <QSettings>

// --- Version ------------------------------------------------------------
// settings_files.h 0.0.38

// --- Version history ----------------------------------------------------
// v0.0.38: Initial release (D-026)

namespace chrono_vault {

// Chrono Vault keeps what it stores in two files, side by side in its folder
// (~/.config/chrono-vault/ on Linux, %APPDATA%\chrono-vault\ on Windows), never
// in the registry (D-026).

// chrono-vault.ini: every setting the settings window can change, and what is
// stored on the fly (window sizes, places and states, View choices, the
// updater's answers). Deleting it, or --reset, brings back the defaults.
class SettingsFile : public QSettings
{
public:
    SettingsFile();
};

// chrono-vault.conf: the vaults as they are configured: the list, the last one,
// keyfile places, fingerprint keys kept, the master password times and the
// wrong-password tries with their waits. --reset never touches it.
class VaultsFile : public QSettings
{
public:
    VaultsFile();
};

// --reset on the command line: chrono-vault.ini is deleted before anything
// reads it (D-026). Returns false when it is there and could not be deleted.
bool deleteSettingsFile();

} // namespace chrono_vault
