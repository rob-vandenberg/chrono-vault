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
#include <QUrl>

class QFileDialog;

// --- Version ------------------------------------------------------------
// places_here.h 0.2.51

// --- Version history ----------------------------------------------------
// v0.2.51: Initial release, after Chrono Viewer's (D-055)

// **The places a file dialog shows down its left side** (D-055), as Chrono
// Viewer has them: the desktop's own list on Linux, the one its file manager
// shows, the places the person added included; on Windows This PC, the
// usual folders and every drive. Nothing here asks a disk whether a folder
// is there: a share that does not answer would hold the window (OR-5).
namespace chrono_vault::places_here {

// What the dialogs are given. Linux: / first, then the desktop's own list,
// or the usual folders where there is none. Windows: This PC, the usual
// folders, every drive.
[[nodiscard]] QList<QUrl> everyPlace();

// Where the desktop keeps its list; empty where there is none (Windows).
[[nodiscard]] QString whereTheDesktopKeepsItsPlaces();

// The folders of such a list: only places on this machine (mounted cloud
// drives among them), not the ones the person hid, each once, at most 60;
// nothing from a file larger than any such list is (OR-4).
[[nodiscard]] QList<QUrl> placesInFile(const QString& file);

// The list of places opens as wide as its widest entry, at most a third of
// the dialog.
void showEveryPlaceWhole(QFileDialog& dialog);

} // namespace chrono_vault::places_here
