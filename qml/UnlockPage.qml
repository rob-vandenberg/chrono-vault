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
// UnlockPage.qml 0.0.3

// --- Version history ----------------------------------------------------
// v0.0.3: Initial release

// The locked vault: the master password opens it.
Item {
    id: unlockPage

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
        spacing: 12
        width: Math.min(parent.width - 32, 420)

        Label {
            text: VaultController.vaultName
            font.pointSize: 18
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
        TextField {
            id: passwordField
            placeholderText: qsTr("Master password")
            echoMode: TextInput.Password
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

    Dialog {
        id: leaveDialog
        title: qsTr("A change is not saved")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Discard | Dialog.Cancel
        ColumnLayout {
            Label {
                text: qsTr("This vault holds a change that could not be saved yet; unlocking it saves it. If you open another vault now, the change is lost.")
                wrapMode: Text.Wrap
                Layout.preferredWidth: 380
            }
        }
        onDiscarded: {
            leaveDialog.close()
            VaultController.closeVault()
        }
    }
}
