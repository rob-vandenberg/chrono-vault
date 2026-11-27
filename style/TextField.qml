// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// TextField.qml 0.8.121
//
// --- Version history ----------------------------------------------------
// v0.8.121: In a dialog 28 high (D-082)
// v0.6.92: Enlarged by exactly the UI scaling (D-069)
// v0.6.91: The right-click menu (D-068)
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import QtQuick.Controls.Fusion.impl

// A frame of exact size with the text typed in it enlarged by exactly the UI
// scaling (D-069). It answers as the text itself does: the keyboard goes to the
// text, and the text's properties, signals and functions are the frame's.
T.Pane {
    id: control
    property alias text: typed.text
    property alias color: typed.color
    property alias selectionColor: typed.selectionColor
    property alias selectedTextColor: typed.selectedTextColor
    property alias placeholderText: typed.placeholderText
    property alias placeholderTextColor: typed.placeholderTextColor
    property alias horizontalAlignment: typed.horizontalAlignment
    property alias verticalAlignment: typed.verticalAlignment
    property alias wrapMode: typed.wrapMode
    property alias readOnly: typed.readOnly
    property alias selectByMouse: typed.selectByMouse
    property alias inputMethodHints: typed.inputMethodHints
    property alias cursorPosition: typed.cursorPosition
    readonly property alias selectedText: typed.selectedText
    readonly property alias selectionStart: typed.selectionStart
    readonly property alias selectionEnd: typed.selectionEnd
    readonly property alias length: typed.length
    readonly property alias canPaste: typed.canPaste
    readonly property alias preeditText: typed.preeditText
    property alias echoMode: typed.echoMode
    property alias validator: typed.validator
    property alias maximumLength: typed.maximumLength
    readonly property alias displayText: typed.displayText
    readonly property alias acceptableInput: typed.acceptableInput
    signal accepted()
    signal textEdited()
    signal editingFinished()
    function clear() { typed.clear() }
    function selectAll() { typed.selectAll() }
    function select(start: int, end: int) { typed.select(start, end) }
    function deselect() { typed.deselect() }
    function cut() { typed.cut() }
    function copy() { typed.copy() }
    function paste() { typed.paste() }
    function undo() { typed.undo() }
    function redo() { typed.redo() }
    function insert(position: int, words: string) { typed.insert(position, words) }
    function remove(start: int, end: int) { typed.remove(start, end) }

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
                   || Math.max(typed.contentWidth, placeholder.implicitWidth) * UiScale.factor + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             typed.contentHeight * UiScale.factor + topPadding + bottomPadding,
                             placeholder.implicitHeight * UiScale.factor + topPadding + bottomPadding)
    padding: 4 * UiScale.factor
    Layout.preferredWidth: UiScale.whole(implicitWidth)
    Layout.preferredHeight: UiScale.whole(implicitHeight)

    T.TextField {
        id: typed
        // Under what a window lays over the field (the eye of a password).
        parent: control
        focus: true
        width: control.width / UiScale.factor
        height: control.height / UiScale.factor
        scale: UiScale.factor
        transformOrigin: Item.TopLeft
        leftPadding: control.leftPadding / UiScale.factor
        rightPadding: control.rightPadding / UiScale.factor
        topPadding: control.topPadding / UiScale.factor
        bottomPadding: control.bottomPadding / UiScale.factor
        font: control.font
        color: control.palette.text
        selectionColor: control.palette.highlight
        selectedTextColor: control.palette.highlightedText
        placeholderTextColor: control.palette.placeholderText
        verticalAlignment: TextInput.AlignVCenter
        T.ContextMenu.menu: TextMenu { editor: typed }
        onAccepted: control.accepted()
        onTextEdited: control.textEdited()
        onEditingFinished: control.editingFinished()

        Text {
            id: placeholder
            x: typed.leftPadding
            y: typed.topPadding
            width: typed.width - typed.leftPadding - typed.rightPadding
            height: typed.height - typed.topPadding - typed.bottomPadding
            text: typed.placeholderText
            font: typed.font
            color: typed.placeholderTextColor
            horizontalAlignment: typed.effectiveHorizontalAlignment
            verticalAlignment: typed.verticalAlignment
            visible: !typed.length && !typed.preeditText
                     && (!typed.activeFocus || typed.horizontalAlignment !== Qt.AlignHCenter)
            elide: Text.ElideRight
        }
    }

    // In a dialog a field is 28 pixels high, as its buttons (the owner, D-082).
    background: TextFieldBackground {
        implicitWidth: 120 * UiScale.factor
        implicitHeight: (control.Window.window !== null && control.Window.window.dialogDesign === true ? 28 : 24) * UiScale.factor
        control: control
    }
}
