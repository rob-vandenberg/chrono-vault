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
// UnlockPage.qml 0.3.66

// --- Version history ----------------------------------------------------
// v0.4.72: Unlock with Windows Hello as wide as its text, centred (the owner)
// v0.4.71: With the PIN only the logo, "Enter the PIN to unlock", the field and the link (the owner)
// v0.4.70: The PIN, with Use master password instead (D-059)
// v0.3.67: A change kept through a lock is asked about before another vault; the password field cleared when the vault changes
// v0.3.66: Open another vault… in the list, Create a new vault… below, each straight to Select Storage; the place under
//          each vault; the list always below the field (the owner, D-058)
// v0.3.65: The owner's design, bigger and rounded; only the vaults found, the last used struck through when not (D-058)
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
    // The two other tasks, open a vault not listed and create one: Select
    // Storage at once, in the start page's windows (the owner, D-058).
    signal openRequested()
    signal createRequested()
    // The keyboard back to the master password, after a menu (D-052).
    function focusPassword() {
        passwordField.forceActiveFocus()
    }

    // To another vault, or to open or create one: a change kept through a
    // lock is asked about first, as it would be lost (OR-15).
    property var afterLeaving: null
    function leaveForAnother(then) {
        if (VaultController.hasKeptChanges) {
            afterLeaving = then
            leaveDialog.open()
        } else {
            then()
        }
    }

    // The PIN when it can open the vault shown, unless the person chose the
    // master password (D-059).
    property bool masterInstead: false
    readonly property bool usePin: VaultController.pinUnlockReady && !masterInstead

    function unlockNow() {
        if (usePin)
            VaultController.unlockWithPin(passwordField.text)
        else
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
        // A password typed for one vault is never tried on another.
        function onVaultPathChanged() {
            passwordField.clear()
            unlockPage.masterInstead = false
            unlockPage.askFinger()
        }
    }
    Connections {
        target: unlockPage.Window.window
        function onVisibleChanged() { unlockPage.askFinger() }
    }

    // After a try that did not open the vault, typing goes on in the field.
    Connections {
        target: VaultController
        function onStateChanged() {
            if (VaultController.state === VaultController.Unlocked)
                unlockPage.masterInstead = false // the next lock offers the PIN again
            if (VaultController.state === VaultController.Locked && unlockPage.visible)
                passwordField.forceActiveFocus()
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: Math.round(12 * Appearance.factor)
        width: Math.min(parent.width - Math.round(32 * Appearance.factor), Math.round(500 * Appearance.factor))

        Image {
            source: "qrc:/icons/chrono-vault.svg"
            sourceSize: Qt.size(Math.round(160 * Appearance.factor), Math.round(160 * Appearance.factor))
            Layout.preferredWidth: Math.round(160 * Appearance.factor)
            Layout.preferredHeight: Math.round(160 * Appearance.factor)
            Layout.alignment: Qt.AlignHCenter
        }
        // With the PIN: only the logo, the line, the field and the link
        // (the owner, D-059).
        Label {
            objectName: "welcomeLine"
            visible: !unlockPage.usePin
            text: qsTr("Welcome to Chrono Vault")
            font.pointSize: Appearance.fontPointSize * 2.2
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            objectName: "unlockLine"
            text: unlockPage.usePin ? qsTr("Enter the PIN to unlock") : qsTr("Enter the Master Password to unlock")
            font.pointSize: Appearance.fontPointSize * 1.6
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            Layout.topMargin: Math.round(24 * Appearance.factor)
            Layout.bottomMargin: Math.round(8 * Appearance.factor)
        }

        // Every vault Chrono Vault knows, each with its own master password;
        // the one used last is chosen (the owner, D-058).
        ComboBox {
            id: vaultBox
            objectName: "unlockVaultBox"
            visible: !unlockPage.usePin
            readonly property string other: "\u0001other"
            // Only the vaults found; the one used last also when it is not,
            // struck through (the owner, 0.3.65).
            readonly property var listed: VaultController.foundVaults.concat(
                VaultController.missingVault !== "" && VaultController.foundVaults.indexOf(VaultController.missingVault) < 0
                    ? [VaultController.missingVault] : [])
            model: listed.concat([other])
            currentIndex: listed.indexOf(VaultController.vaultPath)
            font.pointSize: Appearance.fontPointSize * 1.4
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(58 * Appearance.factor)
            function placeOf(vaultFile) {
                const place = VaultController.vaultPlace(vaultFile)
                if (!place.onWebDav)
                    return qsTr("this computer")
                const host = String(place.address).replace(/^[a-z]+:\/\//i, "").split("/")[0]
                return host.toLowerCase().indexOf("pcloud") >= 0 ? qsTr("pCloud") : host
            }
            function iconOf(vaultFile) {
                return VaultController.vaultPlace(vaultFile).onWebDav ? "qrc:/icons/storage-webdav.svg"
                                                                       : "qrc:/icons/storage-device.svg"
            }
            contentItem: RowLayout {
                spacing: Math.round(8 * Appearance.factor)
                Item { Layout.preferredWidth: Math.round(6 * Appearance.factor) }
                Image {
                    source: vaultBox.iconOf(VaultController.vaultPath)
                    sourceSize: Qt.size(Math.round(24 * Appearance.factor), Math.round(24 * Appearance.factor))
                    Layout.preferredWidth: Math.round(24 * Appearance.factor)
                    Layout.preferredHeight: Math.round(24 * Appearance.factor)
                }
                Label {
                    text: VaultController.vaultName
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            // Rounded, as the owner's design.
            background: Rectangle {
                radius: Math.round(8 * Appearance.factor)
                color: vaultBox.palette.base
                border.color: vaultBox.activeFocus ? vaultBox.palette.highlight : vaultBox.palette.mid
            }
            // The list opens below the field, as big as it, in a rounded frame
            // (the mockup the owner chose).
            popup: Popup {
                y: vaultBox.height + Math.round(4 * Appearance.factor)
                width: vaultBox.width
                padding: 1
                // Below the field always, scrolling when the room below is short.
                implicitHeight: Math.min(contentItem.implicitHeight + 2,
                                         unlockPage.height - vaultBox.mapToItem(unlockPage, 0, 0).y - vaultBox.height
                                         - Math.round(12 * Appearance.factor))
                margins: 0
                background: Rectangle {
                    radius: Math.round(8 * Appearance.factor)
                    color: vaultBox.palette.base
                    border.color: vaultBox.palette.mid
                }
                contentItem: ListView {
                    clip: true
                    ScrollIndicator.vertical: ScrollIndicator {}
                    implicitHeight: contentHeight
                    model: vaultBox.popup.visible ? vaultBox.delegateModel : null
                    currentIndex: vaultBox.highlightedIndex
                }
            }
            delegate: ItemDelegate {
                id: vaultRow
                required property string modelData
                required property int index
                readonly property bool isOther: modelData === vaultBox.other
                readonly property bool missing: !isOther && VaultController.foundVaults.indexOf(modelData) < 0
                objectName: isOther ? "openOrCreateAnother" : "unlockVault_" + VaultController.vaultNameOf(modelData)
                width: vaultBox.width - 2
                height: isOther ? Math.round(48 * Appearance.factor) : Math.round(60 * Appearance.factor)
                font: vaultBox.font
                highlighted: vaultBox.highlightedIndex === index && !missing
                enabled: !missing
                background: Rectangle {
                    color: vaultRow.highlighted ? vaultBox.palette.highlight : "transparent"
                    opacity: vaultRow.highlighted ? 0.5 : 1
                    radius: Math.round(6 * Appearance.factor)
                    // A line above "Open or create another vault…".
                    Rectangle {
                        visible: vaultRow.isOther
                        anchors.top: parent.top
                        width: parent.width
                        height: 1
                        color: vaultBox.palette.mid
                    }
                }
                contentItem: RowLayout {
                    spacing: Math.round(10 * Appearance.factor)
                    Image {
                        visible: !vaultRow.isOther
                        source: vaultRow.isOther ? "" : vaultBox.iconOf(vaultRow.modelData)
                        sourceSize: Qt.size(Math.round(24 * Appearance.factor), Math.round(24 * Appearance.factor))
                        Layout.preferredWidth: Math.round(24 * Appearance.factor)
                        Layout.preferredHeight: Math.round(24 * Appearance.factor)
                    }
                    Label {
                        visible: vaultRow.isOther
                        text: "+"
                        font: vaultBox.font
                        horizontalAlignment: Text.AlignHCenter
                        Layout.preferredWidth: Math.round(24 * Appearance.factor)
                    }
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        Label {
                            text: vaultRow.isOther ? qsTr("Open another vault…") : VaultController.vaultNameOf(vaultRow.modelData)
                            textFormat: Text.PlainText
                            font.pointSize: vaultBox.font.pointSize
                            font.strikeout: vaultRow.missing
                            opacity: vaultRow.missing ? 0.45 : vaultRow.isOther ? 0.8 : 1
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        // Where it is kept, so two vaults of one name are told apart.
                        Label {
                            visible: !vaultRow.isOther
                            text: vaultRow.isOther ? "" : vaultBox.placeOf(vaultRow.modelData)
                            textFormat: Text.PlainText
                            opacity: 0.6
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                    Label {
                        visible: !vaultRow.isOther && (vaultRow.missing || vaultRow.modelData === VaultController.vaultPath)
                        text: vaultRow.missing ? qsTr("not found") : qsTr("last used")
                        opacity: 0.7
                    }
                }
            }
            onActivated: function(index) {
                const chosen = model[index]
                // The list follows the vault shown again, whatever was picked.
                currentIndex = Qt.binding(function() { return vaultBox.listed.indexOf(VaultController.vaultPath) })
                if (chosen === other)
                    unlockPage.leaveForAnother(function() { unlockPage.openRequested() })
                else if (chosen !== VaultController.vaultPath)
                    unlockPage.leaveForAnother(function() { VaultController.switchToVault(chosen) })
                passwordField.forceActiveFocus()
            }
        }

        RowLayout {
            spacing: 0
            Layout.fillWidth: true
            PasswordField {
                id: passwordField
                objectName: "masterPasswordField"
                placeholderText: unlockPage.usePin ? qsTr("PIN") : qsTr("Master Password")
                font.pointSize: Appearance.fontPointSize * 1.4
                // The placeholder in the normal font, the password in the monospaced one.
                font.family: text === "" ? Appearance.font.family : "Noto Sans Mono"
                leftPadding: Math.round(14 * Appearance.factor)
                // Rounded, as the owner's design.
                background: Rectangle {
                    radius: Math.round(8 * Appearance.factor)
                    color: passwordField.palette.base
                    border.color: passwordField.activeFocus ? passwordField.palette.highlight : passwordField.palette.mid
                }
                Layout.preferredHeight: Math.round(58 * Appearance.factor)
                Layout.fillWidth: true
                // Greyed while waiting after wrong master passwords (D-041).
                enabled: VaultController.unlockWaitSeconds === 0
                onAccepted: unlockPage.unlockNow()
                onEnabledChanged: if (enabled && unlockPage.visible) forceActiveFocus()
            }
        }
        Button {
            objectName: "useMasterPasswordButton"
            visible: unlockPage.usePin
            text: qsTr("Use master password instead")
            flat: true
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                unlockPage.masterInstead = true
                passwordField.clear()
                passwordField.forceActiveFocus()
            }
        }
        BigButton {
            objectName: "createNewButton"
            visible: !unlockPage.usePin
            text: qsTr("Create a new vault…")
            Layout.fillWidth: true
            Layout.topMargin: Math.round(12 * Appearance.factor)
            onClicked: unlockPage.leaveForAnother(function() { unlockPage.createRequested() })
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
            // As wide as its text, like Use master password instead (the owner).
            Layout.alignment: Qt.AlignHCenter
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
        objectName: "leaveVaultDialog"

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
            VaultController.giveUpKeptChangesOnLeaving()
            const then = unlockPage.afterLeaving
            unlockPage.afterLeaving = null
            if (then)
                then()
        }
        onRejected: unlockPage.afterLeaving = null
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
