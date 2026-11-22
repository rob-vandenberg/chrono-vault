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
// WelcomePage.qml 0.0.9

// --- Version history ----------------------------------------------------
// v0.0.9: New vault and Open vault reachable from the File menu (D-025)
// v0.0.7: Text sizes follow the UI scaling (D-023)
// v0.0.4: The program's icon in the upper left; a typo; the eye in the password fields
// v0.0.3: Initial release

// The start: create a new vault, or open one that exists.
Item {
    id: welcomePage

    // File > New vault… and Open vault… (D-025).
    function newVault() { createFileDialog.open() }
    function openVault() { openFileDialog.open() }

    Image {
        source: "qrc:/icons/chrono-vault.svg"
        sourceSize: Qt.size(128, 128)
        width: 128
        height: 128
        x: 16
        y: 16
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16
        width: Math.min(parent.width - 32, 420)

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
            onClicked: createFileDialog.open()
        }
        Button {
            text: qsTr("Open a vault…")
            Layout.fillWidth: true
            onClicked: openFileDialog.open()
        }
    }

    FileDialog {
        id: createFileDialog
        title: qsTr("Where should the new vault be kept?")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "chronovault"
        nameFilters: [qsTr("Chrono Vault vaults (*.chronovault)")]
        onAccepted: {
            if (VaultController.canCreateAt(selectedFile)) {
                newVaultDialog.file = selectedFile
                newVaultDialog.open()
            }
        }
    }

    FileDialog {
        id: openFileDialog
        title: qsTr("Open a vault")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Chrono Vault vaults (*.chronovault)"), qsTr("All files (*)")]
        onAccepted: VaultController.chooseVault(selectedFile)
    }

    DialogWindow {

        id: newVaultDialog

        memoryName: "newVault"

        property url file

        readonly property bool passwordsAgree: firstPassword.text !== "" && firstPassword.text === secondPassword.text

        title: qsTr("Choose the master password")
        standardButtons: Dialog.Ok | Dialog.Cancel

        // OK only when both fields hold the same password, so a typing slip
        // never sends the person back to the start.
        Binding {
            target: newVaultDialog.standardButton(Dialog.Ok)
            property: "enabled"
            value: newVaultDialog.passwordsAgree
        }

        onShown: firstPassword.forceActiveFocus()
        onAccepted: {
            VaultController.createVault(file, firstPassword.text, secondPassword.text)
            firstPassword.clear()
            secondPassword.clear()
        }
        onRejected: {
            firstPassword.clear()
            secondPassword.clear()
        }

        ColumnLayout {
            spacing: 8
            Label {
                text: qsTr("This password opens the vault. Nobody can open it without it, not even you, so choose one you will remember.")
                wrapMode: Text.Wrap
                Layout.preferredWidth: 360
            }
            PasswordField {
                id: firstPassword
                placeholderText: qsTr("Master password")
                Layout.fillWidth: true
            }
            PasswordField {
                id: secondPassword
                placeholderText: qsTr("The same password again")
                Layout.fillWidth: true
                onAccepted: if (newVaultDialog.passwordsAgree) newVaultDialog.accept()
            }
            Label {
                visible: secondPassword.text !== "" && !newVaultDialog.passwordsAgree
                text: qsTr("The two passwords are not the same yet.")
                font.bold: true
            }
        }
    }
}
