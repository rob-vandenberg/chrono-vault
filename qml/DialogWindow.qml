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
// DialogWindow.qml 0.7.111

// --- Version history ----------------------------------------------------
// v0.7.111: A question as tall as its text; Enter only on the selected button, shown; Esc on Cancel or Close; Y and N (D-079)
// v0.6.94: The main window keeps its size, the others scale along (D-070)
// v0.6.92: Exact sizes (D-069)
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
    Component.onCompleted: {
        Appearance.dress(this)
        markLetters()
    }

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
    // The button selected when the window opens, with the keyboard and its
    // blue edge: the highlighted one, or what a window names itself (the safe
    // one, when the other cannot be undone). Enter presses only the selected
    // button (D-079).
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
    // A field to type in anywhere in the window's content (a text only to
    // read, as the licences, is none).
    function hasFields(item) {
        const from = item || contentArea
        for (let i = 0; i < from.children.length; ++i) {
            const one = from.children[i]
            if (one.visible && ((isTyping(one) && !one.readOnly) || hasFields(one)))
                return true
        }
        return false
    }
    // Esc presses Cancel, or Close in a window without a field; nothing else
    // (the owner, D-079).
    function escTarget() {
        const cancel = buttonBox.standardButton(Dialog.Cancel)
        if (cancel && cancel.visible && cancel.enabled)
            return cancel
        const close = buttonBox.standardButton(Dialog.Close)
        if (close && close.visible && close.enabled && !hasFields())
            return close
        return null
    }
    function pressEscape() {
        const target = escTarget()
        if (target)
            target.click()
    }
    // Yes and No without a field: Y and N press them, the letter underlined
    // and the translator's own (D-079).
    readonly property bool asksYesNo: (standardButtons & Dialog.Yes) !== 0 && (standardButtons & Dialog.No) !== 0
    property string yesLetter: ""
    property string noLetter: ""
    function letterOf(text) {
        const at = text.indexOf("&")
        return at >= 0 && at + 1 < text.length ? text.charAt(at + 1).toUpperCase() : ""
    }
    function markLetters() {
        const yes = buttonBox.standardButton(Dialog.Yes)
        const no = buttonBox.standardButton(Dialog.No)
        if (yes)
            yes.text = qsTr("&Yes")
        if (no)
            no.text = qsTr("&No")
        yesLetter = yes ? letterOf(yes.text) : ""
        noLetter = no ? letterOf(no.text) : ""
    }
    onStandardButtonsChanged: markLetters()
    // A window that only asks: exactly as tall as its text, never resized,
    // no size remembered (the owner, D-079).
    property bool fitsContent: false
    // Opened: the keyboard on its button, unless the window put it in a field.
    function focusDefault() {
        if (!isTyping(activeFocusItem) && enterTarget && enterTarget.enabled && enterTarget.visible)
            enterTarget.forceActiveFocus(Qt.TabFocusReason)
    }
    // A button holding the keyboard is the selected one, which Enter presses,
    // so it always shows its blue edge (the owner, D-079). Passed on through
    // the button row, or given back as the window becomes active, it would
    // arrive as "other" or "window", which Qt draws without the edge.
    Connections {
        target: dialogWindow.isButton(dialogWindow.activeFocusItem) ? dialogWindow.activeFocusItem : null
        ignoreUnknownSignals: true
        function onFocusReasonChanged() { dialogWindow.markSelected() }
    }
    onActiveFocusItemChanged: markSelected()
    function markSelected() {
        const on = activeFocusItem
        if (isButton(on) && on.focusReason !== undefined
                && (on.focusReason === Qt.OtherFocusReason || on.focusReason === Qt.ActiveWindowFocusReason))
            on.focusReason = Qt.TabFocusReason
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
    function buttonBox_yes() { return buttonBox.standardButton(Dialog.Yes) }
    function buttonBox_no() { return buttonBox.standardButton(Dialog.No) }
    function selectNext(forward) {
        let next = activeFocusItem
        for (let step = 0; step < 50; ++step) {
            next = next.nextItemInFocusChain(forward)
            if (!next || next === activeFocusItem)
                return
            if (isButton(next) && next.visible && next.enabled) {
                next.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason)
                return
            }
        }
    }
    function standardButton(button) {
        return buttonBox.standardButton(button)
    }

    flags: fitsContent ? Qt.Dialog | Qt.CustomizeWindowHint | Qt.WindowTitleHint | Qt.WindowCloseButtonHint : Qt.Dialog
    modality: Qt.ApplicationModal
    visible: false
    // The edge around everything, the same as the settings window's.
    readonly property real edge: 10 * Appearance.factor
    // The size it first opens at; then the person's own, which scales along
    // with the UI scaling (D-070).
    property real firstWidth: Math.max(400 * Appearance.factor, layout.implicitWidth + 2 * edge)
    property real firstHeight: Math.max(160 * Appearance.factor, layout.implicitHeight + 2 * edge)
    width: fitsContent ? minimumWidth : memory.wide(firstWidth, minimumWidth, memory.own)
    height: fitsContent ? minimumHeight : memory.high(firstHeight, minimumHeight, memory.own)
    // Never narrower than its content can take, so nothing is cut off or
    // pushed outside: texts wrap within their reading width, and the window
    // only grows wider (D-061).
    minimumWidth: fitsContent ? Math.max(400 * Appearance.factor, layout.implicitWidth + 2 * edge)
                              : layout.implicitWidth + 2 * edge
    minimumHeight: layout.implicitHeight + 2 * edge
    maximumWidth: fitsContent ? minimumWidth : 16777215
    maximumHeight: fitsContent ? minimumHeight : 16777215
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
        name: dialogWindow.fitsContent ? "" : dialogWindow.memoryName
    }

    // Only the active window's: a dialog made inside another has this key too,
    // and two of them at once would make neither answer.
    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: Appearance.focusWindow === dialogWindow
        onActivated: dialogWindow.pressEscape()
    }
    Shortcut {
        sequences: [dialogWindow.yesLetter]
        enabled: Appearance.focusWindow === dialogWindow && dialogWindow.asksYesNo && dialogWindow.yesLetter !== ""
        onActivated: if (!dialogWindow.hasFields()) dialogWindow.buttonBox_yes().click()
    }
    Shortcut {
        sequences: [dialogWindow.noLetter]
        enabled: Appearance.focusWindow === dialogWindow && dialogWindow.asksYesNo && dialogWindow.noLetter !== ""
        onActivated: if (!dialogWindow.hasFields()) dialogWindow.buttonBox_no().click()
    }

    ExactColumn {
        id: layout
        anchors.fill: parent
        anchors.margins: dialogWindow.edge
        focus: true
        // Esc also through the window's own keys, as Chrono Viewer's QDialog
        // takes it (D-052): it closes the dialog when the shortcut did not.
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Escape && event.modifiers === Qt.NoModifier) {
                event.accepted = true
                dialogWindow.pressEscape()
            } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !event.isAutoRepeat
                       && (event.modifiers === Qt.NoModifier || event.modifiers === Qt.KeypadModifier)) {
                // Enter presses the selected button only; a field that is
                // typed in answers Enter itself (D-079).
                const on = dialogWindow.activeFocusItem
                if (dialogWindow.isButton(on) && on.enabled && on.visible) {
                    event.accepted = true
                    on.click()
                }
            } else if ((event.key === Qt.Key_Left || event.key === Qt.Key_Right) && event.modifiers === Qt.NoModifier
                       && dialogWindow.isButton(dialogWindow.activeFocusItem)) {
                // The arrows move the selection from button to button.
                event.accepted = true
                dialogWindow.selectNext(event.key === Qt.Key_Right)
            }
        }
        gap: 10 * Appearance.factor

        ExactColumn {
            id: contentArea
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        ExactRow {
            Layout.fillWidth: true
            gap: 6 * Appearance.factor
            // The left buttons' area takes the free room, so the right ones
            // stand at the right.
            ExactRow {
                id: leftArea
                gap: 6 * Appearance.factor
                Layout.fillWidth: true
            }
            ExactRow {
                id: rightArea
                gap: 6 * Appearance.factor
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
