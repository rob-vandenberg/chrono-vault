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
// DeleteVaultWindow.qml 0.8.122

// --- Version history ----------------------------------------------------
// v0.8.122: Initial release (D-083)

// Delete… in Settings > Vaults (the owner, D-083): a locked vault is deleted
// for good once its own password (and keyfile) is typed and proven. Delete
// stands apart at the far left in red; Cancel is the default; Enter and Esc
// never delete.
DialogWindow {
    id: deleteWindow
    objectName: "deleteVaultWindow"
    title: qsTr("Delete the vault")

    property string vaultFile: ""
    property string problem: ""
    property bool deleting: false
    readonly property string name: VaultController.vaultNameOf(vaultFile)
    readonly property bool primaryAsked: vaultFile !== "" && vaultFile === VaultController.primaryVault
    readonly property int waitSeconds: VaultController.askWaitSeconds
    readonly property bool busy: deleting || VaultController.askBusy
    readonly property string said: problem !== "" ? problem : VaultController.askMessage

    function askVault(file) {
        problem = ""
        deleting = false
        vaultFile = file
        field.clear()
        VaultController.askFor(file)
        if (!visible)
            open()
        field.forceActiveFocus()
    }
    // The password first proven against the vault's copy on this computer,
    // off the window's thread (OR-5); only then deleted.
    function go() {
        if (field.text === "" || busy || waitSeconds > 0)
            return
        const words = field.text
        field.clear()
        problem = ""
        deleting = true
        VaultController.proveWithPassword(words)
    }

    standardButtons: Dialog.Cancel
    defaultButton: Dialog.Cancel
    leftButtons: Button {
        objectName: "deleteVaultButton"
        text: qsTr("Delete")
        palette.buttonText: "#ff6b6b" // every button with red text (the owner, D-082)
        // Enter never deletes (the owner, D-083).
        property bool enterNeverPresses: true
        enabled: field.text !== "" && !deleteWindow.busy && deleteWindow.waitSeconds === 0
        onClicked: deleteWindow.go()
    }
    onVisibleChanged: {
        if (!visible) {
            field.clear()
            VaultController.stopAsking()
        }
    }
    Connections {
        target: VaultController
        enabled: deleteWindow.visible
        function onProven(file) {
            if (file === deleteWindow.vaultFile && deleteWindow.deleting)
                VaultController.deleteVault(file)
        }
        function onAskChanged() {
            // A wrong password: the dialog stays and says so.
            if (deleteWindow.deleting && !VaultController.askBusy && VaultController.askTrouble)
                deleteWindow.deleting = false
        }
        function onDeleteDone(ok, problem) {
            if (!deleteWindow.deleting)
                return
            deleteWindow.deleting = false
            if (ok) {
                deleteWindow.accept()
            } else {
                deleteWindow.problem = problem
                VaultController.askFor(deleteWindow.vaultFile) // asked again, as new
                field.forceActiveFocus()
            }
        }
    }

    ExactColumn {
        gap: 16 * Appearance.factor
        Layout.fillWidth: true
        ExactColumn {
            gap: 8 * Appearance.factor
            Layout.fillWidth: true
            Label {
                objectName: "deleteWarning"
                text: qsTr("WARNING: You are about to delete the vault “%1”.").arg(deleteWindow.name)
                textFormat: Text.PlainText
                font.bold: true
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                objectName: "dialogText"
                textFormat: Text.RichText
                text: deleteWindow.paragraphs([
                    qsTr("The vault and all information it contains will be permanently lost!"),
                    qsTr("This action cannot be undone. Are you sure you want to delete the vault?")])
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        ExactColumn {
            gap: 8 * Appearance.factor
            Layout.fillWidth: true
            PasswordField {
                id: field
                objectName: "deleteVaultField"
                placeholderText: deleteWindow.primaryAsked ? qsTr("Master Password") : qsTr("Password")
                enabled: deleteWindow.waitSeconds === 0 && !deleteWindow.deleting
                Layout.fillWidth: true
                // Enter in the field never deletes (the owner, D-083).
                onEnabledChanged: if (enabled && deleteWindow.visible) forceActiveFocus()
            }
            Label {
                objectName: "deleteVaultWait"
                visible: deleteWindow.waitSeconds > 0
                text: qsTr("Try again in %1").arg(Math.floor(deleteWindow.waitSeconds / 60) + ":"
                                                  + String(deleteWindow.waitSeconds % 60).padStart(2, "0"))
                Layout.fillWidth: true
            }
            // The keyfile, when this vault has one, as unlocking asks it (Q7).
            ExactRow {
                visible: VaultController.askKeyfileNeeded
                Layout.fillWidth: true
                Label {
                    objectName: "deleteVaultKeyfile"
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
            Label {
                objectName: "deleteVaultProblem"
                visible: text !== ""
                text: deleteWindow.said
                textFormat: Text.PlainText
                color: deleteWindow.problem !== "" || VaultController.askTrouble ? "#e53e3e" : deleteWindow.palette.windowText
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
    }

    FilePicker {
        id: keyfileDialog
        memoryName: "UnlockPage.unlockKeyfileDialog"
        parentWindow: deleteWindow
        title: qsTr("Choose the vault's keyfile")
        fileMode: FileDialog.OpenFile
        onAccepted: {
            VaultController.setAskKeyfile(selectedFile)
            field.forceActiveFocus()
        }
    }
}
