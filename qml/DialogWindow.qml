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
// DialogWindow.qml 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Initial release

// A dialog as a window of its own (D-020): movable and resizable, it remembers
// its size and place under its name and opens centred over the main window the
// first time. Its buttons and answers are those of Qt's own Dialog.
Window {
    id: dialogWindow

    property string memoryName: ""
    property alias standardButtons: buttonBox.standardButtons
    readonly property bool opened: visible
    default property alias content: contentArea.data
    property bool answered: false

    signal shown()
    signal accepted()
    signal rejected()
    signal discarded()

    function open() {
        answered = false
        if (!memory.hasSaved() && transientParent) {
            x = transientParent.x + (transientParent.width - width) / 2
            y = transientParent.y + (transientParent.height - height) / 2
        }
        show()
        raise()
        requestActivate()
        shown()
    }
    function accept() {
        answered = true
        hide()
        accepted()
    }
    function reject() {
        answered = true
        hide()
        rejected()
    }
    function discard() {
        answered = true
        discarded()
    }
    function standardButton(button) {
        return buttonBox.standardButton(button)
    }

    flags: Qt.Dialog
    modality: Qt.ApplicationModal
    visible: false
    width: Math.max(400, layout.implicitWidth + 32)
    height: Math.max(160, layout.implicitHeight + 32)
    minimumWidth: Math.min(layout.implicitWidth + 32, 320)
    minimumHeight: layout.implicitHeight + 32
    color: palette.window

    // Closed with its own title bar button: the same as Cancel.
    onClosing: if (!answered) { answered = true; rejected() }

    WindowMemory {
        id: memory
        window: dialogWindow
        name: dialogWindow.memoryName
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: dialogWindow.reject()
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        ColumnLayout {
            id: contentArea
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        DialogButtonBox {
            id: buttonBox
            Layout.fillWidth: true
            onAccepted: dialogWindow.accept()
            onRejected: dialogWindow.reject()
            onDiscarded: dialogWindow.discard()
        }
    }
}
