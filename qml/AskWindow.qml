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
// AskWindow.qml 0.8.129

// --- Version history ----------------------------------------------------
// v0.8.129: Forget, Disconnect and Delete: their words; the proof handed on to the warning that follows (D-060)
// v0.8.128: The action as one word, its title, heading and button in whole sentences (D-073)
// v0.8.125: typedOnly: the password typed, never a finger (D-043)
// v0.8.121: The new PIN 16 apart (D-082)
// v0.8.120: The design of every dialog: its text and normal fields, 440 wide, no size remembered (the owner, D-082)
// v0.7.107: No PIN face; Set or Change PIN; the password of any vault but the primary (D-076)
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

    // The vault asked for, "" for something else; the words in the field's place then.
    property string vaultFile: ""
    property string name: ""
    property string picture: "qrc:/icons/places-webdav.svg"
    property string problem: ""
    property bool withList: false
    // Set or Change PIN: a new PIN typed with the master password (D-076).
    property bool withNewPin: false
    // Only the password typed, never a finger: a finger never sets a PIN nor turns itself on (D-043).
    property bool typedOnly: withNewPin
    // "Remember my credentials" (the owner, D-074).
    property bool mayRemember: false
    readonly property bool remembers: rememberBox.wanted && rememberBox.enabled && rememberBox.checked
    // The primary asks its master password, any other vault its password (the owner, D-076).
    readonly property bool primaryAsked: vaultFile !== "" && vaultFile === VaultController.primaryVault
    // What the password is for, one word; the dialog says it in whole
    // sentences, for translators (the owner, D-073): unlock, export, import,
    // pin, fingerprint, primary, retry, webdav, forget, disconnect, delete.
    property string action: "unlock"
    // Set PIN or Change PIN.
    property bool changesPin: false
    // The Master Password asked where no vault is proven here (Retry sync).
    property bool masterNamed: false
    readonly property bool master: primaryAsked || masterNamed || (!proving && action === "unlock")
    readonly property var words: {
        const m = askWindow.master
        switch (askWindow.action) {
        case "export":
            return [qsTr("Export"), m ? qsTr("Enter the Master Password to export") : qsTr("Enter the password to export"), qsTr("Export")]
        case "import":
            return [qsTr("Import"), m ? qsTr("Enter the Master Password of the vault to import from")
                                      : qsTr("Enter the password of the vault to import from"), qsTr("Import")]
        case "pin":
            return askWindow.changesPin
                   ? [qsTr("Change PIN"), m ? qsTr("Enter the Master Password to change the PIN") : qsTr("Enter the password to change the PIN"), qsTr("Change PIN")]
                   : [qsTr("Set PIN"), m ? qsTr("Enter the Master Password to set the PIN") : qsTr("Enter the password to set the PIN"), qsTr("Set PIN")]
        case "fingerprint":
            return Qt.platform.os === "windows"
                   ? [qsTr("Use Windows Hello to unlock vault"), qsTr("Enter the Master Password to turn on Windows Hello"), qsTr("Turn on")]
                   : [qsTr("Use fingerprint to unlock vault"), qsTr("Enter the Master Password to turn on the fingerprint"), qsTr("Turn on")]
        case "primary":
            return [qsTr("Make primary vault"), qsTr("Enter the Master Password to make “%1” your primary vault").arg(askWindow.newPrimaryName), qsTr("Make primary")]
        case "retry":
            return [qsTr("Retry sync"), m ? qsTr("Enter the Master Password of the vault the items come from")
                                          : qsTr("Enter the password of the vault the items come from"), qsTr("Retry")]
        case "forget":
            return [qsTr("Forget the vault"), m ? qsTr("Enter the Master Password to forget the vault") : qsTr("Enter the password to forget the vault"), qsTr("Forget")]
        case "disconnect":
            return [qsTr("Disconnect the vault"), m ? qsTr("Enter the Master Password to disconnect the vault") : qsTr("Enter the password to disconnect the vault"), qsTr("Disconnect")]
        case "delete":
            return [qsTr("Delete the vault"), qsTr("Enter the password of the vault you are about to delete."), qsTr("Delete")]
        case "webdav":
            return [qsTr("WebDAV password"), qsTr("Enter the password of the WebDAV server"), qsTr("OK")]
        default:
            return [qsTr("Unlock the vault"), m ? qsTr("Enter the Master Password to unlock") : qsTr("Enter the password to unlock"), qsTr("Unlock")]
        }
    }
    // Make primary: the vault that is to be crowned.
    property string newPrimaryName: ""
    readonly property string heading: words[1]
    // Delete says "Password", for the primary too (the owner, D-083).
    readonly property string hint: action === "webdav" || action === "delete" ? qsTr("Password") : master ? qsTr("Master Password") : qsTr("Password")
    readonly property string okText: words[2]
    readonly property bool proving: vaultFile !== ""
    readonly property int waitSeconds: proving ? VaultController.askWaitSeconds : 0
    readonly property bool pinReady: !withNewPin || newPin.text.length >= VaultController.pinShortest
    // The open vault's owner sets its PIN without its password (the owner, D-076).
    readonly property bool ownerSetsPin: withNewPin && proving && VaultController.vaultPath === vaultFile
                                         && VaultController.state === VaultController.Unlocked
    readonly property string newPinText: newPin.text
    readonly property bool busy: proving && VaultController.askBusy
    readonly property string said: problem !== "" ? problem : proving ? VaultController.askMessage : ""
    // The vault was proven; the words were typed.
    signal right(string vaultFile)
    signal entered(string text)
    // Another vault picked in the list.
    signal picked(string vaultFile)

    // The proof kept when this dialog closes, for the warning that follows
    // (Forget, Disconnect); that warning wipes it when it closes (D-060).
    property bool proofHandedOn: false
    function handOn() {
        proofHandedOn = true
        accept()
    }
    function askVault(file) {
        proofHandedOn = false
        problem = ""
        name = ""
        vaultFile = file
        field.clear()
        newPin.clear()
        VaultController.askFor(file)
        rememberBox.checked = VaultController.askRememberPossible && VaultController.remembersFor(file)
        if (!visible)
            open()
        if (ownerSetsPin)
            newPin.forceActiveFocus()
        else
            field.forceActiveFocus()
        fingerWanted = !typedOnly && !ownerSetsPin // the owner's PIN asks no proof
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
        if (ownerSetsPin) {
            if (pinReady)
                right(vaultFile)
            return
        }
        if (field.text === "" || busy || waitSeconds > 0 || !pinReady)
            return
        const words = field.text
        field.clear()
        problem = ""
        if (!proving)
            entered(words)
        else
            VaultController.proveWithPassword(words)
    }

    title: words[0]
    standardButtons: Dialog.Cancel
    rightButtons: Button {
        objectName: "askOk"
        text: askWindow.okText
        enabled: (field.text !== "" || askWindow.ownerSetsPin) && !askWindow.busy && askWindow.waitSeconds === 0 && askWindow.pinReady
        highlighted: true
        onClicked: askWindow.go()
    }
    // Nothing typed or proven is left behind.
    onVisibleChanged: {
        if (!visible) {
            field.clear()
            newPin.clear()
            vaultBox.popup.close()
            if (proving && !proofHandedOn)
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

    // The design of every dialog: its text, then the fields (the owner, D-082).
    ExactColumn {
        gap: 8 * Appearance.factor
        Layout.fillWidth: true
        Label {
            objectName: "askHeading"
            visible: !askWindow.ownerSetsPin
            text: askWindow.heading
            wrapMode: Text.Wrap
            Layout.fillWidth: true
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
            large: false
            Layout.preferredHeight: 28 * Appearance.factor
            onActivated: function(index) {
                const chosen = model[index]
                currentIndex = Qt.binding(function() { return vaultBox.listed.indexOf(vaultBox.shown) })
                if (chosen !== askWindow.vaultFile)
                    askWindow.picked(chosen)
                field.forceActiveFocus()
            }
        }
        PasswordField {
            id: field
            visible: !askWindow.ownerSetsPin
            objectName: "askField"
            placeholderText: askWindow.hint
            enabled: askWindow.waitSeconds === 0
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
                readonly property bool wanted: askWindow.mayRemember && askWindow.proving
                                               && VaultController.askRememberShown
                enabled: VaultController.askRememberPossible
                text: qsTr("Remember my credentials")
                onToggled: VaultController.setRemembersFor(askWindow.vaultFile, checked)
            }
        }
        PasswordField {
            id: newPin
            objectName: "askNewPin"
            visible: askWindow.withNewPin
            placeholderText: qsTr("New PIN (at least %1 characters)").arg(VaultController.pinShortest)
            Layout.fillWidth: true
            // The new PIN apart from the password (the owner, D-082).
            Layout.topMargin: 8 * Appearance.factor
            onAccepted: askWindow.go()
        }
        Label {
            objectName: "askWaitText"
            visible: askWindow.waitSeconds > 0
            text: qsTr("Try again in %1").arg(Math.floor(askWindow.waitSeconds / 60) + ":"
                                              + String(askWindow.waitSeconds % 60).padStart(2, "0"))
            Layout.fillWidth: true
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
            visible: askWindow.proving && VaultController.askFingerOn && !askWindow.typedOnly && !askWindow.ownerSetsPin
            text: qsTr("Unlock with %1").arg(VaultController.quickUnlockName)
            flat: true
            onClicked: VaultController.proveWithFinger()
        }
        Label {
            objectName: "askProblem"
            visible: text !== ""
            text: askWindow.said
            textFormat: Text.PlainText
            color: askWindow.problem !== "" || VaultController.askTrouble ? "#e53e3e" : askWindow.palette.windowText
            wrapMode: Text.Wrap
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
