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
// UnlockPage.qml 0.0.7

// --- Version history ----------------------------------------------------
// v0.0.7: Text sizes follow the UI scaling (D-023)
// v0.0.4: The program's icon in the upper left; the eye in the password field
// v0.0.3: Initial release

// The locked vault: the master password opens it.
Item {
    id: unlockPage

    Image {
        source: "qrc:/icons/chrono-vault.svg"
        sourceSize: Qt.size(Math.round(128 * Appearance.factor), Math.round(128 * Appearance.factor))
        width: Math.round(128 * Appearance.factor)
        height: Math.round(128 * Appearance.factor)
        x: 16
        y: 16
    }

    function unlockNow() {
        VaultController.unlock(passwordField.text)
        passwordField.clear()
    }

    onVisibleChanged: if (visible) passwordField.forceActiveFocus()

    // After a try that did not open the vault, typing goes on in the field.
    Connections {
        target: VaultController
        function onStateChanged() {
            if (VaultController.state === VaultController.Locked && unlockPage.visible)
                passwordField.forceActiveFocus()
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: Math.round(12 * Appearance.factor)
        width: Math.min(parent.width - Math.round(32 * Appearance.factor), Math.round(420 * Appearance.factor))

        Label {
            text: VaultController.vaultName
            font.pointSize: Appearance.fontPointSize * 1.8
            elide: Text.ElideMiddle
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
        Label {
            text: VaultController.vaultPath
            elide: Text.ElideMiddle
            opacity: 0.7
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
        PasswordField {
            id: passwordField
            objectName: "masterPasswordField"
            placeholderText: qsTr("Master password")
            Layout.fillWidth: true
            onAccepted: unlockPage.unlockNow()
        }
        Button {
            text: qsTr("Unlock")
            Layout.fillWidth: true
            onClicked: unlockPage.unlockNow()
        }
        Label {
            visible: VaultController.hasKeptChanges
            text: qsTr("A change could not be saved before the vault locked. Unlock to save it.")
            font.bold: true
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Button {
            text: qsTr("Open another vault…")
            flat: true
            Layout.alignment: Qt.AlignHCenter
            onClicked: VaultController.hasKeptChanges ? leaveDialog.open() : VaultController.closeVault()
        }
    }

    DialogWindow {

        id: leaveDialog

        memoryName: "leaveVault"
        title: qsTr("A change is not saved")
        standardButtons: Dialog.Discard | Dialog.Cancel
        ColumnLayout {
            Label {
                text: qsTr("This vault holds a change that could not be saved yet; unlocking it saves it. If you open another vault now, the change is lost.")
                wrapMode: Text.Wrap
                Layout.preferredWidth: Math.round(380 * Appearance.factor)
            }
        }
        onDiscarded: {
            leaveDialog.close()
            VaultController.closeVault()
        }
    }
}
