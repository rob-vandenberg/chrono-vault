// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// TextMenu.qml 0.6.92
//
// --- Version history ----------------------------------------------------
// v0.6.92: Enlarged by exactly the UI scaling (D-069)
// v0.6.91: Initial release (D-068)

import QtQuick

// The right-click menu of every text field (D-068): what cannot be done is
// grey. What a password field hides is never cut or copied.
Menu {
    id: control
    objectName: "textMenu"
    required property Item editor
    readonly property bool hides: editor.echoMode !== undefined && editor.echoMode !== TextInput.Normal
    readonly property bool chosen: editor.selectedText !== ""
    // Its field has the keyboard, so what is chosen stays seen as chosen.
    onAboutToShow: editor.forceActiveFocus(Qt.MouseFocusReason)
    MenuItem {
        objectName: "textCut"
        text: qsTr("Cut")
        enabled: control.chosen && !control.hides && !control.editor.readOnly
        onTriggered: control.editor.cut()
    }
    MenuItem {
        objectName: "textCopy"
        text: qsTr("Copy")
        enabled: control.chosen && !control.hides
        onTriggered: control.editor.copy()
    }
    MenuItem {
        objectName: "textPaste"
        text: qsTr("Paste")
        enabled: control.editor.canPaste && !control.editor.readOnly
        onTriggered: control.editor.paste()
    }
    MenuSeparator {}
    MenuItem {
        objectName: "textSelectAll"
        text: qsTr("Select All")
        enabled: control.editor.length > 0 && control.editor.selectedText.length < control.editor.length
        onTriggered: control.editor.selectAll()
    }
}
