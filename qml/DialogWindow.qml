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
// DialogWindow.qml 0.5.82

// --- Version history ----------------------------------------------------
// v0.5.82: Esc only of the active dialog; Enter presses the button, which has the keyboard when a question opens (D-062)
// v0.5.78: Never narrower than its content can take (D-061)
// v0.1.43: Esc also through the window's own keys, as Chrono Viewer's QDialog
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
    // The button that Enter presses and that holds the keyboard when the
    // window opens: the highlighted one, or what a window names itself (the
    // safe one, when the other cannot be undone).
    property int defaultButton: Dialog.NoButton
    property Item defaultItem: null
    readonly property Item enterTarget: {
        if (defaultItem)
            return defaultItem
        for (let i = 0; i < rightArea.children.length; ++i) {
            const own = rightArea.children[i]
            if (own.highlighted === true && own.visible)
                return own
        }
        const wanted = defaultButton !== Dialog.NoButton ? [defaultButton]
                     : [Dialog.Ok, Dialog.Save, Dialog.Yes, Dialog.Close, Dialog.Cancel, Dialog.No] // never Discard
        const all = buttonBox.standardButtons
        for (const one of wanted) {
            if ((all & one) && buttonBox.standardButton(one))
                return buttonBox.standardButton(one)
        }
        return null
    }
    // A button, not a tick box or a choice.
    function isButton(item) {
        return item && typeof item.click === "function" && !item.indicator
    }
    function isTyping(item) {
        return item && item.cursorPosition !== undefined
    }
    // Opened: the keyboard on its button, unless the window put it in a field.
    function focusDefault() {
        if (!isTyping(activeFocusItem) && enterTarget && enterTarget.enabled && enterTarget.visible)
            enterTarget.forceActiveFocus(Qt.TabFocusReason)
    }

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
        focusDefault()
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
    // Never narrower than its content can take, so nothing is cut off or
    // pushed outside: texts wrap within their reading width, and the window
    // only grows wider (D-061).
    minimumWidth: layout.implicitWidth + 2 * edge
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

    // Only the active window's: a dialog made inside another has this key too,
    // and two of them at once would make neither answer.
    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: Appearance.focusWindow === dialogWindow
        onActivated: dialogWindow.reject()
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: dialogWindow.edge
        focus: true
        // Esc also through the window's own keys, as Chrono Viewer's QDialog
        // takes it (D-052): it closes the dialog when the shortcut did not.
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Escape && event.modifiers === Qt.NoModifier) {
                event.accepted = true
                dialogWindow.reject()
            } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !event.isAutoRepeat
                       && (event.modifiers === Qt.NoModifier || event.modifiers === Qt.KeypadModifier)) {
                // Enter presses the button that has the keyboard, else the
                // window's own; a field that is typed in answers Enter itself.
                const on = dialogWindow.activeFocusItem
                const target = dialogWindow.isButton(on) ? on
                             : dialogWindow.isTyping(on) ? null : dialogWindow.enterTarget
                if (target && target.enabled && target.visible) {
                    event.accepted = true
                    target.click()
                }
            }
        }
        spacing: Math.round(10 * Appearance.factor)

        ColumnLayout {
            id: contentArea
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Math.round(6 * Appearance.factor)
            // The left buttons' area takes the free room, so the right ones
            // stand at the right.
            RowLayout {
                id: leftArea
                spacing: Math.round(6 * Appearance.factor)
                Layout.fillWidth: true
            }
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
