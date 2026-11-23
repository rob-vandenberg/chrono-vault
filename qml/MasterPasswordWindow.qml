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
// MasterPasswordWindow.qml 0.2.51

// --- Version history ----------------------------------------------------
// v0.2.51: Its file dialogs as Chrono Viewer's (D-055)
// v0.0.22: OK greyed until ready: set once the button exists, as the earlier binding never took hold
// v0.0.21: The keyfile choosers belong to this window, so they show
// v0.0.15: Initial release (D-031)

// File > Change master password or keyfile… (D-031): the current master
// password, a new one twice, and a keyfile or none. Nothing changes until
// the new vault is written and checked (OR-15).
DialogWindow {
    id: passwordWindow
    memoryName: "masterPassword"
    title: qsTr("Change master password or keyfile")
    standardButtons: Dialog.Ok | Dialog.Cancel

    property url keyfile
    readonly property bool passwordsAgree: newPassword.text !== "" && newPassword.text === repeatedPassword.text
    readonly property bool ready: currentPassword.text !== "" && passwordsAgree
                                  && (!useKeyfile.checked || keyfile.toString() !== "")

    function ask() {
        currentPassword.clear()
        newPassword.clear()
        repeatedPassword.clear()
        useKeyfile.checked = VaultController.usesKeyfile
        keyfile = VaultController.usesKeyfile ? VaultController.keyfileUrl : ""
        open()
        currentPassword.forceActiveFocus()
    }

    // Set once the button exists; a binding to it made earlier never took hold.
    Component.onCompleted: passwordWindow.standardButton(Dialog.Ok).enabled = Qt.binding(function() { return passwordWindow.ready })

    onAccepted: {
        VaultController.changeMasterPassword(currentPassword.text, newPassword.text, repeatedPassword.text,
                                             useKeyfile.checked ? keyfile : "")
        currentPassword.clear()
        newPassword.clear()
        repeatedPassword.clear()
    }
    onRejected: {
        currentPassword.clear()
        newPassword.clear()
        repeatedPassword.clear()
    }

    ColumnLayout {
        spacing: Math.round(8 * Appearance.factor)
        PasswordField {
            id: currentPassword
            objectName: "currentPasswordField"
            placeholderText: qsTr("Current master password")
            Layout.preferredWidth: Math.round(360 * Appearance.factor)
            Layout.fillWidth: true
        }
        PasswordField {
            id: newPassword
            objectName: "newPasswordField"
            placeholderText: qsTr("New master password")
            Layout.fillWidth: true
        }
        PasswordField {
            id: repeatedPassword
            objectName: "repeatedPasswordField"
            placeholderText: qsTr("The same password again")
            Layout.fillWidth: true
            onAccepted: if (passwordWindow.ready) passwordWindow.accept()
        }
        Label {
            visible: repeatedPassword.text !== "" && !passwordWindow.passwordsAgree
            text: qsTr("The two passwords are not the same yet.")
            font.bold: true
        }
        CheckBox {
            id: useKeyfile
            objectName: "changeUseKeyfileBox"
            text: qsTr("Also lock the vault with a keyfile")
        }
        ColumnLayout {
            visible: useKeyfile.checked
            Label {
                text: qsTr("Without its keyfile the vault cannot be opened, not even with the right password. Keep a copy of the keyfile somewhere safe, apart from the vault.")
                font.bold: true
                wrapMode: Text.Wrap
                Layout.preferredWidth: Math.round(360 * Appearance.factor)
            }
            Label {
                text: passwordWindow.keyfile.toString() === "" ? qsTr("No keyfile chosen yet.")
                      : qsTr("Keyfile: %1").arg(decodeURIComponent(passwordWindow.keyfile.toString().replace(/^.*\//, "")))
                elide: Text.ElideMiddle
                opacity: 0.8
                Layout.preferredWidth: Math.round(360 * Appearance.factor)
            }
            RowLayout {
                Button {
                    text: qsTr("Choose a keyfile…")
                    onClicked: chooseDialog.open()
                }
                Button {
                    text: qsTr("Make a new keyfile…")
                    onClicked: makeDialog.open()
                }
            }
        }
    }

    FilePicker {
        id: chooseDialog
        memoryName: "MasterPasswordWindow.chooseDialog"
        parentWindow: passwordWindow
        title: qsTr("Choose a keyfile")
        fileMode: FileDialog.OpenFile
        onAccepted: passwordWindow.keyfile = selectedFile
    }
    FilePicker {
        id: makeDialog
        memoryName: "MasterPasswordWindow.makeDialog"
        parentWindow: passwordWindow
        title: qsTr("Where should the new keyfile be kept?")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "chronovaultkey"
        nameFilters: [qsTr("Chrono Vault keyfiles (*.chronovaultkey)")]
        onAccepted: VaultController.makeKeyfile(selectedFile)
    }
    Connections {
        target: VaultController
        function onKeyfileMade(made, ok) {
            if (ok && passwordWindow.visible)
                passwordWindow.keyfile = makeDialog.selectedFile
        }
    }
}
