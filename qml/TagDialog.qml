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
import QtQuick.Controls
import QtQuick.Layouts

// --- Version ------------------------------------------------------------
// TagDialog.qml 0.8.120

// --- Version history ----------------------------------------------------
// v0.8.120: The one width of every dialog (D-082)
// v0.6.92: Exact sizes (D-069)
// v0.5.83: Initial release, out of VaultPage.qml (D-063)

// Renames a tag and the tags nested under it, in every item (D-028): from the
// sidebar's right-click and from Settings > Tags.
DialogWindow {
    id: tagDialog
    memoryName: "renameTag"
    property string tag: ""
    title: qsTr("Rename tag")
    standardButtons: Dialog.Ok | Dialog.Cancel
    function ask(chosenTag) {
        tag = chosenTag
        tagName.text = chosenTag
        open()
        tagName.forceActiveFocus()
        tagName.selectAll()
    }
    ExactColumn {
        Label {
            text: qsTr("A slash nests a tag under another, as in “Work/Servers”.")
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        TextField {
            id: tagName
            objectName: "tagNameField"
            Layout.fillWidth: true
            onAccepted: tagDialog.accept()
        }
    }
    onAccepted: VaultController.renameTag(tag, tagName.text)
}
