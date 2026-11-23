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
// WelcomePage.qml 0.3.62

// --- Version history ----------------------------------------------------
// v0.3.62: The Select Storage window with its tiles; a failed new WebDAV vault reopens the WebDAV window (D-057)
// v0.3.60: A new vault, or one opened, on this computer or on a WebDAV server (D-057)
// v0.2.51: Its file dialogs as Chrono Viewer's (D-055)
// v0.0.22: OK greyed until ready: set once the button exists, as the earlier binding never took hold
// v0.0.15: A new vault may also be locked with a keyfile, chosen or made; its password may be saved in the primary
//          vault (D-030, D-031)
// v0.0.9: New vault and Open vault reachable from the File menu (D-025)
// v0.0.7: Text sizes follow the UI scaling (D-023)
// v0.0.4: The program's icon in the upper left; a typo; the eye in the password fields
// v0.0.3: Initial release

// The start: create a new vault, or open one that exists.
Item {
    id: welcomePage

    // File > New vault… and Open vault… (D-025).
    // Where the vault is kept is asked first (ref, D-057).
    function newVault() { storageChoice.mode = "new"; storageChoice.open() }
    function openVault() { storageChoice.mode = "open"; storageChoice.open() }

    Image {
        source: "qrc:/icons/chrono-vault.svg"
        sourceSize: Qt.size(Math.round(128 * Appearance.factor), Math.round(128 * Appearance.factor))
        width: Math.round(128 * Appearance.factor)
        height: Math.round(128 * Appearance.factor)
        x: 16
        y: 16
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: Math.round(16 * Appearance.factor)
        width: Math.min(parent.width - Math.round(32 * Appearance.factor), Math.round(420 * Appearance.factor))

        Label {
            text: qsTr("Welcome to Chrono Vault")
            font.pointSize: Appearance.fontPointSize * 1.8
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: qsTr("Create a new vault, or open one you already have.")
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
        Button {
            text: qsTr("Create a new vault…")
            Layout.fillWidth: true
            onClicked: welcomePage.newVault()
        }
        Button {
            text: qsTr("Open a vault…")
            Layout.fillWidth: true
            onClicked: welcomePage.openVault()
        }
    }

    // Where the vault is kept: a tile per storage (ref, REQUIREMENTS.md 5.4.1).
    StorageWindow {
        id: storageChoice
        onChosen: function(storage) {
            if (storage === "device") {
                if (storageChoice.mode === "new")
                    createFileDialog.open()
                else
                    openFileDialog.open()
            } else {
                webDavWindow.mode = storageChoice.mode
                webDavWindow.open()
            }
        }
    }

    WebDavWindow {
        id: webDavWindow
        onNewVaultNamed: function(name) {
            newVaultDialog.webDavName = name
            newVaultDialog.open()
        }
    }

    // A new WebDAV vault that did not reach the server: nothing was kept, and
    // the WebDAV window comes back with the reason (D-057).
    Connections {
        target: VaultController
        function onWebDavCreateFailed(problem) {
            webDavWindow.mode = "new"
            webDavWindow.openWithProblem(problem)
        }
    }

    FilePicker {
        id: createFileDialog
        memoryName: "WelcomePage.createFileDialog"
        parentWindow: welcomePage
        title: qsTr("Where should the new vault be kept?")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "chronovault"
        nameFilters: [qsTr("Chrono Vault vaults (*.chronovault)")]
        onAccepted: {
            if (VaultController.canCreateAt(selectedFile)) {
                newVaultDialog.webDavName = ""
                newVaultDialog.file = selectedFile
                newVaultDialog.open()
            }
        }
    }

    FilePicker {
        id: openFileDialog
        memoryName: "WelcomePage.openFileDialog"
        parentWindow: welcomePage
        title: qsTr("Open a vault")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Chrono Vault vaults (*.chronovault)"), qsTr("All files (*)")]
        onAccepted: VaultController.chooseVault(selectedFile)
    }

    DialogWindow {

        id: newVaultDialog
        objectName: "newVaultDialog"

        memoryName: "newVault"

        property url file
        // The new vault's name on the WebDAV server, "" for a file here (D-057).
        property string webDavName: ""
        property url keyfile
        // Asked when the window opens: whether the primary vault is unlocked.
        property bool offerSave: false

        readonly property bool passwordsAgree: firstPassword.text !== "" && firstPassword.text === secondPassword.text
        readonly property bool ready: passwordsAgree && (!useKeyfile.checked || keyfile.toString() !== "")

        title: qsTr("Choose the master password")
        standardButtons: Dialog.Ok | Dialog.Cancel

        // OK only when both fields hold the same password, so a typing slip
        // never sends the person back to the start.
        // Set once the button exists; a binding to it made earlier never took hold.
        Component.onCompleted: newVaultDialog.standardButton(Dialog.Ok).enabled = Qt.binding(function() { return newVaultDialog.ready })

        onShown: {
            offerSave = VaultController.canSaveInPrimary()
            saveInPrimary.checked = true
            useKeyfile.checked = false
            keyfile = ""
            firstPassword.forceActiveFocus()
        }
        onAccepted: {
            if (webDavName !== "")
                VaultController.createWebDavVault(webDavName, firstPassword.text, secondPassword.text,
                                                  useKeyfile.checked ? keyfile : "", offerSave && saveInPrimary.checked)
            else
                VaultController.createVault(file, firstPassword.text, secondPassword.text,
                                            useKeyfile.checked ? keyfile : "", offerSave && saveInPrimary.checked)
            firstPassword.clear()
            secondPassword.clear()
        }
        onRejected: {
            firstPassword.clear()
            secondPassword.clear()
        }

        ColumnLayout {
            spacing: Math.round(8 * Appearance.factor)
            Label {
                text: qsTr("This password opens the vault. Nobody can open it without it, not even you, so choose one you will remember.")
                wrapMode: Text.Wrap
                Layout.preferredWidth: Math.round(360 * Appearance.factor)
            }
            PasswordField {
                id: firstPassword
                objectName: "newVaultPassword"
                placeholderText: qsTr("Master password")
                Layout.fillWidth: true
            }
            PasswordField {
                id: secondPassword
                objectName: "newVaultPasswordAgain"
                placeholderText: qsTr("The same password again")
                Layout.fillWidth: true
                onAccepted: if (newVaultDialog.ready) newVaultDialog.accept()
            }
            Label {
                visible: secondPassword.text !== "" && !newVaultDialog.passwordsAgree
                text: qsTr("The two passwords are not the same yet.")
                font.bold: true
            }
            // A keyfile beside the password (D-031).
            CheckBox {
                id: useKeyfile
                objectName: "useKeyfileBox"
                text: qsTr("Also lock the vault with a keyfile")
            }
            ColumnLayout {
                visible: useKeyfile.checked
                spacing: Math.round(6 * Appearance.factor)
                Label {
                    text: qsTr("Without its keyfile the vault cannot be opened, not even with the right password. Keep a copy of the keyfile somewhere safe, apart from the vault.")
                    font.bold: true
                    wrapMode: Text.Wrap
                    Layout.preferredWidth: Math.round(360 * Appearance.factor)
                }
                Label {
                    objectName: "chosenKeyfileLabel"
                    text: newVaultDialog.keyfile.toString() === "" ? qsTr("No keyfile chosen yet.")
                          : qsTr("Keyfile: %1").arg(decodeURIComponent(newVaultDialog.keyfile.toString().replace(/^.*\//, "")))
                    elide: Text.ElideMiddle
                    opacity: 0.8
                    Layout.preferredWidth: Math.round(360 * Appearance.factor)
                }
                RowLayout {
                    Button {
                        text: qsTr("Choose a keyfile…")
                        onClicked: keyfileOpenDialog.open()
                    }
                    Button {
                        text: qsTr("Make a new keyfile…")
                        onClicked: keyfileSaveDialog.open()
                    }
                }
            }
            CheckBox {
                id: saveInPrimary
                objectName: "saveInPrimaryBox"
                visible: newVaultDialog.offerSave
                text: qsTr("Save this vault's password in “%1”").arg(VaultController.vaultNameOf(VaultController.primaryVault))
            }
        }
    }

    FilePicker {
        id: keyfileOpenDialog
        memoryName: "WelcomePage.keyfileOpenDialog"
        parentWindow: welcomePage
        title: qsTr("Choose a keyfile")
        fileMode: FileDialog.OpenFile
        onAccepted: newVaultDialog.keyfile = selectedFile
    }

    FilePicker {
        id: keyfileSaveDialog
        memoryName: "WelcomePage.keyfileSaveDialog"
        parentWindow: welcomePage
        title: qsTr("Where should the new keyfile be kept?")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "chronovaultkey"
        nameFilters: [qsTr("Chrono Vault keyfiles (*.chronovaultkey)")]
        onAccepted: VaultController.makeKeyfile(selectedFile)
    }

    Connections {
        target: VaultController
        function onKeyfileMade(keyfile, ok) {
            if (ok && newVaultDialog.visible)
                newVaultDialog.keyfile = keyfileSaveDialog.selectedFile
        }
    }
}
