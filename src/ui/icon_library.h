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

#include <QList>
#include <QString>
#include <QStringList>

// --- Version ------------------------------------------------------------
// icon_library.h 0.0.40

// --- Version history ----------------------------------------------------
// v0.0.40: Initial release (D-050)

namespace chrono_vault {

// The icons that ship with Chrono Vault, offered in the icon picker (D-050).
// They are installed beside the program (icons/), and copied at each start
// into the person's own folder, where the picker reads them; nothing already
// there is ever written over, so what the person changed stays.

// The person's own folder: ~/.local/share/chrono-vault/icons on Linux,
// %LOCALAPPDATA%\chrono-vault\icons on Windows.
[[nodiscard]] QString personalIconsFolder();
// The folder installed beside the program.
[[nodiscard]] QString installedIconsFolder();
// For the tests: other folders than the two above.
void setIconsFoldersForTests(const QString& installed, const QString& personal);

// Copies every file and folder of the installed icons that the person's
// folder lacks; nothing there is replaced. Returns false when something
// could not be copied (a full disk, no rights); what could be copied is.
bool copyMissingIcons();

// One section of the picker: a folder named in icons.md, its heading (as
// written there, not yet translated) and its pictures, sorted by name.
struct IconSection
{
    QString folder;
    QString heading;
    QStringList files; // full paths
};

// Reads icons.md in the given folder: one line per section, "folder:
// Heading", in the order shown. Empty lines, lines without a colon, and
// folders that are not there or not a plain name are skipped.
[[nodiscard]] QList<IconSection> readIconSections(const QString& folder);

} // namespace chrono_vault
