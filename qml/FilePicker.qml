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

import QtQuick
import QtQuick.Dialogs

// --- Version ------------------------------------------------------------
// FilePicker.qml 0.2.51

// --- Version history ----------------------------------------------------
// v0.2.51: Initial release (D-055)

// A file dialog as Chrono Viewer's (D-055), in place of Qt Quick's own,
// which on Linux is Qt's fallback with a fixed path and few places. It has
// the properties and signals of Qt's FileDialog that Chrono Vault uses, so
// a window only names it differently; FilePickers shows the dialog.
QtObject {
    id: picker
    property string memoryName: ""
    property string title: ""
    property int fileMode: FileDialog.OpenFile
    property var nameFilters: []
    property string defaultSuffix: ""
    property url selectedFile
    property url currentFolder: FilePickers.homeFolder()
    // The window it opens over: an item of that window, or the window.
    property var parentWindow: null

    signal accepted()
    signal rejected()

    // A lock while the dialog is open closes it as Cancel: what it was asked
    // for belongs to a vault that is now locked.
    property bool picking: false
    property Connections lockWatcher: Connections {
        target: VaultController
        function onStateChanged() {
            if (picker.picking)
                FilePickers.closeOpenDialog()
        }
    }

    function open() {
        const stateBefore = VaultController.state
        picking = true
        const chosen = FilePickers.pick(memoryName, title, fileMode === FileDialog.SaveFile, nameFilters,
                                        defaultSuffix, selectedFile, currentFolder, parentWindow)
        picking = false
        if (chosen.toString() === "" || VaultController.state !== stateBefore) {
            rejected()
            return
        }
        selectedFile = chosen
        // The next time it opens where the person went, as Qt's own does.
        const path = chosen.toString()
        currentFolder = path.substring(0, path.lastIndexOf("/"))
        accepted()
    }
}
