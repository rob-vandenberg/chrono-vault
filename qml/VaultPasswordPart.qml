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
// VaultPasswordPart.qml 0.8.128

// --- Version history ----------------------------------------------------
// v0.8.128: Initial release: Forget and Disconnect ask a closed or locked vault's password in their own dialog (D-060)

// The password of a vault that is not the open one, asked inside a dialog that
// already says what it does, as Delete does (the owner, D-060, D-083). The open
// vault's owner needs no password: then the part is not shown at all.
ExactColumn {
    id: part
    gap: 8 * Appearance.factor
    Layout.fillWidth: true

    property string vaultFile: ""
    // "forget" or "disconnect": the line above the field.
    property string action: ""
    property bool proving: false
    // An ask of this part is open: closed whatever became of the vault, so
    // nothing proven stays behind.
    property bool asking: false
    // A vault whose file is not on this computer cannot be proven: not asked.
    readonly property bool needed: vaultFile !== "" && VaultController.foundVaults.indexOf(vaultFile) >= 0
                                   && !(VaultController.vaultPath === vaultFile && VaultController.state === VaultController.Unlocked)
    readonly property bool typed: field.text !== ""
    readonly property int waitSeconds: VaultController.askWaitSeconds
    readonly property bool busy: proving || VaultController.askBusy
    readonly property bool ready: !needed || (typed && !busy && waitSeconds === 0)
    readonly property bool primaryAsked: vaultFile === VaultController.primaryVault
    signal proven(string vaultFile)

    visible: needed

    function start(file) {
        proving = false
        vaultFile = file
        field.clear()
        if (needed) {
            asking = true
            VaultController.askFor(file)
            field.forceActiveFocus()
        }
    }
    function stop() {
        field.clear()
        proving = false
        if (asking)
            VaultController.stopAsking()
        asking = false
    }
    // The password proven against the vault's copy on this computer, off the
    // window's thread (OR-5); a wrong one is said here and nothing happens.
    function go() {
        if (!needed) {
            proven(vaultFile)
            return
        }
        if (!ready)
            return
        const words = field.text
        field.clear()
        proving = true
        VaultController.proveWithPassword(words)
    }
    Connections {
        target: VaultController
        enabled: part.visible && part.proving
        function onProven(file) {
            if (file === part.vaultFile) {
                part.proving = false
                part.proven(file)
            }
        }
        function onAskChanged() {
            if (!VaultController.askBusy && VaultController.askTrouble)
                part.proving = false
        }
    }

    Label {
        objectName: "vaultPasswordAsks"
        text: part.action === "disconnect"
              ? (part.primaryAsked ? qsTr("Enter the Master Password to disconnect the vault")
                                   : qsTr("Enter the password to disconnect the vault"))
              : (part.primaryAsked ? qsTr("Enter the Master Password to forget the vault")
                                   : qsTr("Enter the password to forget the vault"))
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    PasswordField {
        id: field
        objectName: "vaultPasswordField"
        placeholderText: part.primaryAsked ? qsTr("Master Password") : qsTr("Password")
        enabled: part.waitSeconds === 0 && !part.proving
        Layout.fillWidth: true
        onEnabledChanged: if (enabled && part.visible) forceActiveFocus()
    }
    Label {
        visible: part.waitSeconds > 0
        text: qsTr("Try again in %1").arg(Math.floor(part.waitSeconds / 60) + ":"
                                          + String(part.waitSeconds % 60).padStart(2, "0"))
        Layout.fillWidth: true
    }
    ExactRow {
        visible: VaultController.askKeyfileNeeded
        Layout.fillWidth: true
        Label {
            text: VaultController.askKeyfile === "" ? qsTr("This vault requires its keyfile.")
                  : qsTr("Keyfile: %1").arg(VaultController.askKeyfile)
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }
        Button {
            text: qsTr("Choose keyfile…")
            onClicked: keyfileDialog.open()
        }
    }
    Label {
        objectName: "vaultPasswordProblem"
        visible: text !== ""
        text: VaultController.askMessage
        textFormat: Text.PlainText
        color: VaultController.askTrouble ? "#e53e3e" : part.palette.windowText
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    FilePicker {
        id: keyfileDialog
        memoryName: "UnlockPage.unlockKeyfileDialog"
        parentWindow: part.Window.window
        title: qsTr("Choose the vault's keyfile")
        fileMode: FileDialog.OpenFile
        onAccepted: {
            VaultController.setAskKeyfile(selectedFile)
            field.forceActiveFocus()
        }
    }
}
