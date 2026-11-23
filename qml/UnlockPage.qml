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
// UnlockPage.qml 0.3.64

// --- Version history ----------------------------------------------------
// v0.3.64: As the owner's mockup: the icon and welcome, a dropdown of the vaults (the last used chosen), the field
//          with its arrow; Open or create another vault… in the dropdown (D-058)
// v0.3.62: A WebDAV vault's server address, not its copy's path (D-057)
// v0.2.51: Its file dialogs as Chrono Viewer's (D-055)
// v0.1.42: focusPassword (D-052)
// v0.0.39: The reminder says how often it asks (D-049)
// v0.0.27: The finger asked whenever the page is on screen: at a start, for another vault, back from the tray (D-042)
// v0.0.26: Greyed with "Try again in" while waiting after wrong master passwords (D-041)
// v0.0.23: Unlocking with a fingerprint, asked at once; the yearly master password reminder (D-038)
// v0.0.15: The keyfile, when the vault needs one, chosen here and remembered (D-031); after a lock, the primary
//          vault's master password unlocks every vault (D-030)
// v0.0.7: Text sizes follow the UI scaling (D-023)
// v0.0.4: The program's icon in the upper left; the eye in the password field
// v0.0.3: Initial release

// The locked vault: the master password opens it.
Item {
    id: unlockPage
    // The keyboard back to the master password, after a menu (D-052).
    function focusPassword() {
        passwordField.forceActiveFocus()
    }

    // To the start page, to open or create another vault; a change kept
    // through a lock is asked about first.
    function leaveForAnother() {
        if (VaultController.hasKeptChanges)
            leaveDialog.open()
        else
            VaultController.closeVault()
    }

    function unlockNow() {
        VaultController.unlock(passwordField.text)
        passwordField.clear()
    }

    // The finger asked whenever this page is on screen for a vault with the
    // fingerprint on, unless the yearly reminder is due (D-038, D-042): at a
    // start once the window shows, after a lock, for another vault, back from
    // the tray. Never while the window is hidden.
    function askFinger() {
        if (visible && Window.window && Window.window.visible && VaultController.state === VaultController.Locked
                && VaultController.quickUnlockOn && !VaultController.reminderDue)
            VaultController.unlockWithQuickUnlock()
    }
    onVisibleChanged: {
        if (!visible)
            return
        passwordField.forceActiveFocus()
        askFinger()
    }
    Component.onCompleted: askFinger()
    Connections {
        target: VaultController
        function onVaultPathChanged() { unlockPage.askFinger() }
    }
    Connections {
        target: unlockPage.Window.window
        function onVisibleChanged() { unlockPage.askFinger() }
    }

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

        Image {
            source: "qrc:/icons/chrono-vault.svg"
            sourceSize: Qt.size(Math.round(96 * Appearance.factor), Math.round(96 * Appearance.factor))
            Layout.preferredWidth: Math.round(96 * Appearance.factor)
            Layout.preferredHeight: Math.round(96 * Appearance.factor)
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: qsTr("Welcome to Chrono Vault")
            font.pointSize: Appearance.fontPointSize * 1.8
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: qsTr("Choose your vault and enter its master password")
            opacity: 0.8
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            Layout.bottomMargin: Math.round(8 * Appearance.factor)
        }

        // Every vault Chrono Vault knows, each with its own master password;
        // the one used last is chosen (the owner, D-058).
        Label { text: qsTr("Vault"); opacity: 0.8 }
        ComboBox {
            id: vaultBox
            objectName: "unlockVaultBox"
            readonly property string other: "\u0001other"
            model: VaultController.vaults.concat([other])
            currentIndex: VaultController.vaults.indexOf(VaultController.vaultPath)
            Layout.fillWidth: true
            function iconOf(vaultFile) {
                return VaultController.vaultPlace(vaultFile).onWebDav ? "qrc:/icons/storage-webdav.svg"
                                                                       : "qrc:/icons/storage-device.svg"
            }
            contentItem: RowLayout {
                spacing: Math.round(8 * Appearance.factor)
                Item { Layout.preferredWidth: Math.round(2 * Appearance.factor) }
                Image {
                    source: vaultBox.iconOf(VaultController.vaultPath)
                    sourceSize: Qt.size(Math.round(20 * Appearance.factor), Math.round(20 * Appearance.factor))
                    Layout.preferredWidth: Math.round(20 * Appearance.factor)
                    Layout.preferredHeight: Math.round(20 * Appearance.factor)
                }
                Label {
                    text: VaultController.vaultName
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            delegate: ItemDelegate {
                id: vaultRow
                required property string modelData
                required property int index
                readonly property bool isOther: modelData === vaultBox.other
                objectName: isOther ? "openOrCreateAnother" : "unlockVault_" + VaultController.vaultNameOf(modelData)
                width: vaultBox.width
                highlighted: vaultBox.highlightedIndex === index
                contentItem: RowLayout {
                    spacing: Math.round(8 * Appearance.factor)
                    Image {
                        visible: !vaultRow.isOther
                        source: vaultRow.isOther ? "" : vaultBox.iconOf(vaultRow.modelData)
                        sourceSize: Qt.size(Math.round(20 * Appearance.factor), Math.round(20 * Appearance.factor))
                        Layout.preferredWidth: Math.round(20 * Appearance.factor)
                        Layout.preferredHeight: Math.round(20 * Appearance.factor)
                    }
                    Label {
                        text: vaultRow.isOther ? qsTr("Open or create another vault…") : VaultController.vaultNameOf(vaultRow.modelData)
                        textFormat: Text.PlainText
                        opacity: vaultRow.isOther ? 0.8 : 1
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        visible: !vaultRow.isOther && vaultRow.modelData === VaultController.vaultPath
                        text: qsTr("last used")
                        opacity: 0.7
                    }
                }
            }
            onActivated: function(index) {
                const chosen = model[index]
                // The list follows the vault shown again, whatever was picked.
                currentIndex = Qt.binding(function() { return VaultController.vaults.indexOf(VaultController.vaultPath) })
                if (chosen === other)
                    unlockPage.leaveForAnother()
                else if (chosen !== VaultController.vaultPath)
                    VaultController.switchToVault(chosen)
                passwordField.forceActiveFocus()
            }
        }

        Label { text: qsTr("Master password"); opacity: 0.8 }
        RowLayout {
            spacing: 0
            Layout.fillWidth: true
            PasswordField {
                id: passwordField
                objectName: "masterPasswordField"
                placeholderText: qsTr("Master password of %1").arg(VaultController.vaultName)
                Layout.fillWidth: true
                // Greyed while waiting after wrong master passwords (D-041).
                enabled: VaultController.unlockWaitSeconds === 0
                onAccepted: unlockPage.unlockNow()
                onEnabledChanged: if (enabled && unlockPage.visible) forceActiveFocus()
            }
            Button {
                objectName: "unlockButton"
                text: "→"
                Accessible.name: qsTr("Unlock")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Unlock")
                enabled: VaultController.unlockWaitSeconds === 0
                Layout.preferredHeight: passwordField.implicitHeight
                Layout.preferredWidth: passwordField.implicitHeight * 1.2
                onClicked: unlockPage.unlockNow()
            }
        }
        Label {
            objectName: "unlockWaitText"
            visible: VaultController.unlockWaitSeconds > 0
            text: qsTr("Try again in %1").arg(Math.floor(VaultController.unlockWaitSeconds / 60) + ":"
                                              + String(VaultController.unlockWaitSeconds % 60).padStart(2, "0"))
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
        // The keyfile, when this vault needs one (D-031).
        RowLayout {
            visible: VaultController.keyfileNeeded
            Layout.fillWidth: true
            Label {
                objectName: "unlockKeyfileLabel"
                text: VaultController.keyfilePath === "" ? qsTr("This vault needs its keyfile.")
                      : qsTr("Keyfile: %1").arg(VaultController.keyfilePath)
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Choose keyfile…")
                onClicked: unlockKeyfileDialog.open()
            }
        }
        Button {
            objectName: "quickUnlockButton"
            visible: VaultController.quickUnlockOn && !VaultController.reminderDue
            text: qsTr("Unlock with %1").arg(VaultController.quickUnlockName)
            flat: true
            Layout.fillWidth: true
            onClicked: VaultController.unlockWithQuickUnlock()
        }
        // Now and then the master password, so it is not forgotten, as often
        // as Settings > Security says (D-049).
        Label {
            objectName: "reminderText"
            visible: VaultController.reminderDue
            text: [qsTr("Please type your master password: Chrono Vault asks for it once a day, so that you do not forget it."),
                   qsTr("Please type your master password: Chrono Vault asks for it once a month, so that you do not forget it."),
                   qsTr("Please type your master password: Chrono Vault asks for it once a quarter, so that you do not forget it."),
                   qsTr("Please type your master password: Chrono Vault asks for it once a year, so that you do not forget it.")
                  ][Math.min(VaultController.masterPasswordReminder, 3)]
            wrapMode: Text.Wrap
            font.bold: true
            Layout.fillWidth: true
        }
        Button {
            objectName: "notNowButton"
            visible: VaultController.reminderDue
            text: qsTr("Not now")
            flat: true
            Layout.alignment: Qt.AlignHCenter
            onClicked: VaultController.dismissReminder()
        }
        Label {
            visible: VaultController.hasKeptChanges
            text: qsTr("A change could not be saved before the vault locked. Unlock to save it.")
            font.bold: true
            wrapMode: Text.Wrap
            Layout.fillWidth: true
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

    FilePicker {
        id: unlockKeyfileDialog
        memoryName: "UnlockPage.unlockKeyfileDialog"
        parentWindow: unlockPage
        title: qsTr("Choose the vault's keyfile")
        fileMode: FileDialog.OpenFile
        onAccepted: {
            VaultController.keyfilePath = selectedFile
            passwordField.forceActiveFocus()
        }
    }
}
