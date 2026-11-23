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
#include <QStringList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// --- Version ------------------------------------------------------------
// file_pickers.h 0.2.51

// --- Version history ----------------------------------------------------
// v0.2.51: Initial release, after Chrono Viewer's file pickers (D-055)

namespace chrono_vault {

// Every file dialog of Chrono Vault (D-055), as Chrono Viewer's: Qt's own
// widget dialog, never the desktop's, with an address field the person can
// type a path into, the desktop's own places down its left side
// (places_here.h), and its size and place remembered under its own name.
// It follows the program's theme and font, which are set for the whole
// application.
class FilePickers : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // Shows the dialog and waits for the person: the file chosen, or an
    // empty address when they cancelled. `save` asks for a file to write
    // (which may not exist yet); otherwise for an existing one. `window` is
    // the window it opens over (an item or a window of it).
    Q_INVOKABLE QUrl pick(const QString& memoryName, const QString& title, bool save, const QStringList& nameFilters,
                          const QString& defaultSuffix, const QUrl& selectedFile, const QUrl& folder,
                          QObject* window);

    // Closes the dialog that is open, as Cancel does: the vault it was
    // asked for has locked (D-055).
    Q_INVOKABLE void closeOpenDialog();

    // The person's home folder, where a dialog opens when nothing else says.
    [[nodiscard]] Q_INVOKABLE QUrl homeFolder() const;
};

} // namespace chrono_vault
