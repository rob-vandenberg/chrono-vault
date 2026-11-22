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
// DialogWindow.qml 0.0.9

// --- Version history ----------------------------------------------------
// v0.0.9: Opened quietly, without taking the keyboard; a window that cannot be closed for a while; buttons of its own at the right (D-024)
// v0.0.8: A narrow edge, as the settings window's; a place for the dialog's own buttons at the bottom left
// v0.0.7: An application window with the program's font, so the UI scaling reaches it (D-023)
// v0.0.4: Initial release

// A dialog as a window of its own (D-020): movable and resizable, it remembers
// its size and place under its name and opens centred over the main window the
// first time. Its buttons and answers are those of Qt's own Dialog.
// An application window, not a plain one: only that carries a font, which
// the UI scaling needs to reach the dialog's controls (D-023).
ApplicationWindow {
    id: dialogWindow
    font: Appearance.font
    // The program's colours, never the desktop theme's (D-023).
    Component.onCompleted: Appearance.dress(this)

    property string memoryName: ""
    property alias standardButtons: buttonBox.standardButtons
    readonly property bool opened: visible
    default property alias content: contentArea.data
    // Buttons of the dialog's own, at the bottom left (as Check for update in
    // About); the standard buttons stay at the bottom right.
    property alias leftButtons: leftArea.data
    // Buttons of the dialog's own at the bottom right, before the standard ones
    // (as Download and Skip this update before Cancel).
    property alias rightButtons: rightArea.data
    property bool answered: false

    signal shown()
    signal accepted()
    signal rejected()
    signal discarded()

    // Shown without taking the keyboard from someone who is typing (an update
    // found by itself, D-024).
    function openQuietly() {
        answered = false
        if (!memory.hasSaved() && transientParent) {
            x = transientParent.x + (transientParent.width - width) / 2
            y = transientParent.y + (transientParent.height - height) / 2
        }
        show()
        shown()
    }
    // A link for a label: the words (already translated) escaped, never read
    // as markup.
    function link(address, words) {
        const escape = function(text) {
            return String(text).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;")
        }
        return "<a href=\"" + escape(address) + "\">" + escape(words) + "</a>"
    }

    // While false the window cannot be closed (the system is installing).
    property bool closable: true

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
        if (!closable)
            return
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
    // The edge around everything, the same as the settings window's.
    readonly property int edge: Math.round(10 * Appearance.factor)
    width: Math.max(Math.round(400 * Appearance.factor), layout.implicitWidth + 2 * edge)
    height: Math.max(Math.round(160 * Appearance.factor), layout.implicitHeight + 2 * edge)
    minimumWidth: Math.min(layout.implicitWidth + 2 * edge, Math.round(320 * Appearance.factor))
    minimumHeight: layout.implicitHeight + 2 * edge
    color: palette.window

    // Closed with its own title bar button: the same as Cancel.
    onClosing: function(close) {
        if (!closable) {
            close.accepted = false
            return
        }
        if (!answered) {
            answered = true
            rejected()
        }
    }

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
        anchors.margins: dialogWindow.edge
        spacing: Math.round(10 * Appearance.factor)

        ColumnLayout {
            id: contentArea
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Math.round(6 * Appearance.factor)
            RowLayout {
                id: leftArea
                spacing: Math.round(6 * Appearance.factor)
            }
            Item { Layout.fillWidth: true }
            RowLayout {
                id: rightArea
                spacing: Math.round(6 * Appearance.factor)
            }
            DialogButtonBox {
                id: buttonBox
                // No edge of its own: the window's edge is the only one.
                padding: 0
                onAccepted: dialogWindow.accept()
                onRejected: dialogWindow.reject()
                onDiscarded: dialogWindow.discard()
            }
        }
    }
}
