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
import QtQuick.Dialogs
import QtQuick.Layouts

// --- Version ------------------------------------------------------------
// AskWindow.qml 0.6.105

// --- Version history ----------------------------------------------------
// v0.6.105: Another primary vault; Remember my credentials; Open, Closed, Locked (D-074)
// v0.6.103: Initial release (D-073)

// The one dialog that asks a master password, a PIN or a server's password, in
// the look of the start-up page (the owner, D-073). For a vault it is proven
// right here, with the finger too where that is on, and "right" is said with
// what was proven kept until the dialog closes; for anything else the words
// typed are given with "entered", and the one who asked says what is wrong.
DialogWindow {
    id: askWindow
    objectName: "askWindow"
    memoryName: "ask"

    // The vault asked for, "" for something else; the words in the field's place then.
    property string vaultFile: ""
    property string name: ""
    property string picture: "qrc:/icons/storage-webdav.svg"
    property string problem: ""
    property bool withList: false
    property bool pinAllowed: false
    // "Remember my credentials" (the owner, D-074).
    property bool mayRemember: false
    readonly property bool remembers: rememberBox.wanted && rememberBox.enabled && rememberBox.checked
    property string heading: qsTr("Enter the Master Password to unlock")
    property string hint: qsTr("Master Password")
    property string okText: qsTr("Unlock")
    readonly property bool proving: vaultFile !== ""
    property bool masterInstead: false
    readonly property bool usePin: proving && pinAllowed && !masterInstead && VaultController.askPinReady
    readonly property int waitSeconds: proving ? VaultController.askWaitSeconds : 0
    readonly property bool busy: proving && VaultController.askBusy
    readonly property string said: problem !== "" ? problem : proving ? VaultController.askMessage : ""
    // The vault was proven; the words were typed.
    signal right(string vaultFile)
    signal entered(string text)
    // Another vault picked in the list.
    signal picked(string vaultFile)

    function askVault(file) {
        problem = ""
        masterInstead = false
        name = ""
        vaultFile = file
        field.clear()
        VaultController.askFor(file)
        rememberBox.checked = VaultController.askRememberPossible && VaultController.remembersFor(file)
        if (!visible)
            open()
        field.forceActiveFocus()
        fingerWanted = true
        askFinger()
    }
    // Once, and only when this window is active, so Windows lets Windows Hello
    // come on top; a cancelled one hands the activation back, which must not
    // ask again (the owner, 0.4.73).
    property bool fingerWanted: false
    function askFinger() {
        if (fingerWanted && active && proving) {
            fingerWanted = false
            VaultController.proveWithFinger()
        }
    }
    onActiveChanged: askFinger()
    function ask(what, wrong) {
        vaultFile = ""
        name = what
        problem = wrong
        field.clear()
        open()
        field.forceActiveFocus()
    }
    function go() {
        if (field.text === "" || busy || waitSeconds > 0)
            return
        const words = field.text
        field.clear()
        problem = ""
        if (!proving)
            entered(words)
        else if (usePin)
            VaultController.proveWithPin(words)
        else
            VaultController.proveWithPassword(words)
    }

    title: qsTr("Unlock the vault")
    standardButtons: Dialog.Cancel
    firstWidth: 534 * Appearance.factor
    firstHeight: Math.max(271 * Appearance.factor, minimumHeight)
    rightButtons: Button {
        objectName: "askOk"
        text: askWindow.okText
        enabled: field.text !== "" && !askWindow.busy && askWindow.waitSeconds === 0
        highlighted: true
        onClicked: askWindow.go()
    }
    // Nothing typed or proven is left behind.
    onVisibleChanged: {
        if (!visible) {
            field.clear()
            vaultBox.popup.close()
            if (proving)
                VaultController.stopAsking()
        }
    }
    Connections {
        target: VaultController
        enabled: askWindow.visible && askWindow.proving
        function onProven(file) {
            if (file === askWindow.vaultFile)
                askWindow.right(file)
        }
    }

    ExactColumn {
        gap: 12 * Appearance.factor
        Layout.fillWidth: true
        Layout.leftMargin: 8 * Appearance.factor
        Layout.rightMargin: 8 * Appearance.factor
        Label {
            objectName: "askHeading"
            text: askWindow.usePin ? qsTr("Enter the PIN to unlock") : askWindow.heading
            font.pointSize: Appearance.fontPointSize * 1.6
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            Layout.topMargin: 6 * Appearance.factor
            Layout.bottomMargin: 8 * Appearance.factor
        }
        VaultBox {
            id: vaultBox
            objectName: "askVaultBox"
            listed: askWindow.withList ? VaultController.foundVaults : [askWindow.vaultFile]
            shown: askWindow.vaultFile
            isList: askWindow.withList && listed.length > 1
            withPadlocks: true
            otherName: askWindow.name
            otherPicture: askWindow.picture
            Layout.fillWidth: true
            Layout.preferredHeight: 58 * Appearance.factor
            onActivated: function(index) {
                const chosen = model[index]
                currentIndex = Qt.binding(function() { return vaultBox.listed.indexOf(vaultBox.shown) })
                if (chosen !== askWindow.vaultFile)
                    askWindow.picked(chosen)
                field.forceActiveFocus()
            }
        }
        BigPasswordField {
            id: field
            objectName: "askField"
            placeholderText: askWindow.usePin ? qsTr("PIN") : askWindow.hint
            enabled: askWindow.waitSeconds === 0
            Layout.preferredHeight: 58 * Appearance.factor
            Layout.fillWidth: true
            onAccepted: askWindow.go()
            onEnabledChanged: if (enabled && askWindow.visible) forceActiveFocus()
        }
        Item {
            visible: rememberBox.wanted
            implicitWidth: rememberBox.implicitWidth
            implicitHeight: rememberBox.implicitHeight
            Layout.preferredHeight: implicitHeight
            HoverHandler { id: overRemember }
            // Gray, it says why.
            ToolTip.visible: overRemember.hovered && !rememberBox.enabled
            ToolTip.text: qsTr("Your primary vault is locked. Open it first.")
            CheckBox {
                id: rememberBox
                objectName: "askRemember"
                readonly property bool wanted: askWindow.mayRemember && askWindow.proving && !askWindow.usePin
                                               && VaultController.askRememberShown
                enabled: VaultController.askRememberPossible
                text: qsTr("Remember my credentials")
                onToggled: VaultController.setRemembersFor(askWindow.vaultFile, checked)
            }
        }
        Button {
            objectName: "askMasterInstead"
            visible: askWindow.usePin
            text: qsTr("Use master password instead")
            flat: true
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                askWindow.masterInstead = true
                field.clear()
                field.forceActiveFocus()
            }
        }
        Label {
            objectName: "askWaitText"
            visible: askWindow.waitSeconds > 0
            text: qsTr("Try again in %1").arg(Math.floor(askWindow.waitSeconds / 60) + ":"
                                              + String(askWindow.waitSeconds % 60).padStart(2, "0"))
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
        // The keyfile, when this vault needs one (D-031).
        ExactRow {
            visible: askWindow.proving && VaultController.askKeyfileNeeded
            Layout.fillWidth: true
            Label {
                objectName: "askKeyfileLabel"
                text: VaultController.askKeyfile === "" ? qsTr("This vault needs its keyfile.")
                      : qsTr("Keyfile: %1").arg(VaultController.askKeyfile)
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Choose keyfile…")
                onClicked: keyfileDialog.open()
            }
        }
        Button {
            objectName: "askFinger"
            visible: askWindow.proving && VaultController.askFingerOn
            text: qsTr("Unlock with %1").arg(VaultController.quickUnlockName)
            flat: true
            Layout.alignment: Qt.AlignHCenter
            onClicked: VaultController.proveWithFinger()
        }
        Label {
            objectName: "askProblem"
            visible: text !== ""
            text: askWindow.said
            textFormat: Text.PlainText
            color: askWindow.problem !== "" || VaultController.askTrouble ? "#e53e3e" : askWindow.palette.windowText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
    }

    FilePicker {
        id: keyfileDialog
        memoryName: "UnlockPage.unlockKeyfileDialog"
        parentWindow: askWindow
        title: qsTr("Choose the vault's keyfile")
        fileMode: FileDialog.OpenFile
        onAccepted: {
            VaultController.setAskKeyfile(selectedFile)
            field.forceActiveFocus()
        }
    }
}
