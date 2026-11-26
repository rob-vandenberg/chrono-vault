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
// UnlockPage.qml 0.7.111

// --- Version history ----------------------------------------------------
// v0.7.111: Question dialogs fit their text (D-079)
// v0.7.107: The PIN of the vault logged in with, the list with it, "Unlock using password instead"; the password of any vault but the primary (D-076)
// v0.6.106: Close and Lock per vault; the primary first; back to the vault after a lock (D-075)
// v0.6.103: One standard dialog for a master password, a PIN or a finger (D-073)
// v0.6.92: Exact sizes (D-069)
// v0.6.90: The list of vaults on the background of every pop-up (D-067)
// v0.5.82: Enter never throws a kept change away (D-062)
// v0.5.78: Texts in dialogs wrap with the window (D-061)
// v0.4.73: The finger asked only once the window is active (the owner)
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
        if (closed)
            openButton.forceActiveFocus()
        else
            passwordField.forceActiveFocus()
    }
    // Closed: the vault shown opens without a password (D-075).
    readonly property bool closed: VaultController.keptRevision >= 0 && VaultController.vaults.length >= 0
                                   && VaultController.state === VaultController.Locked
                                   && VaultController.vaultStateOf(VaultController.vaultPath) === 1
    onClosedChanged: if (visible && VaultController.state === VaultController.Locked) focusPassword()

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

    // The PIN after a lock, of the vault logged in with, unless the person chose
    // the password (D-059, D-076).
    property bool masterInstead: false
    readonly property bool usePin: VaultController.pinUnlockReady && !masterInstead && !closed
    readonly property bool primaryShown: VaultController.vaultPath === VaultController.primaryVault

    function unlockNow() {
        if (usePin)
            VaultController.unlockWithPin(passwordField.text)
        else
            VaultController.unlock(passwordField.text, rememberBox.visible && rememberBox.checked)
        passwordField.clear()
    }

    // The finger asked whenever this page is on screen for a vault with the
    // fingerprint on, unless the yearly reminder is due (D-038, D-042): at a
    // start once the window shows, after a lock, for another vault, back from
    // the tray. Never while the window is hidden.
    // Only once the window is active, so Windows lets Windows Hello come on
    // top (the owner, 0.4.73).
    // Asked once each time the page is shown: a cancelled Windows Hello hands
    // the window its activation back, which must not ask again (0.4.73).
    property bool fingerAsked: false
    function askFinger() {
        if (fingerAsked || closed)
            return
        if (visible && Window.window && Window.window.visible && Window.window.active
                && VaultController.state === VaultController.Locked
                && VaultController.quickUnlockOn && !VaultController.reminderDue)
        {
            fingerAsked = true
            VaultController.unlockWithQuickUnlock()
        }
    }
    onVisibleChanged: {
        if (!visible) {
            fingerAsked = false
            return
        }
        rememberBox.read()
        focusPassword()
        askFinger()
    }
    Component.onCompleted: askFinger()
    Connections {
        target: VaultController
        // A password typed for one vault is never tried on another.
        function onVaultPathChanged() {
            passwordField.clear()
            rememberBox.read()
            unlockPage.masterInstead = false
            unlockPage.fingerAsked = false
            unlockPage.askFinger()
        }
    }
    Connections {
        target: unlockPage.Window.window
        function onVisibleChanged() {
            if (!unlockPage.Window.window.visible)
                unlockPage.fingerAsked = false // hidden into the tray: asked again when shown
            unlockPage.askFinger()
        }
        function onActiveChanged() { unlockPage.askFinger() }
    }

    // After a try that did not open the vault, typing goes on in the field.
    Connections {
        target: VaultController
        function onStateChanged() {
            if (VaultController.state === VaultController.Unlocked) {
                unlockPage.masterInstead = false // the next lock offers the PIN again
                unlockPage.fingerAsked = false
            }
            if (VaultController.state === VaultController.Locked && unlockPage.visible)
                unlockPage.focusPassword()
        }
    }

    ExactColumn {
        anchors.centerIn: parent
        gap: 12 * Appearance.factor
        width: Math.min(parent.width - 32 * Appearance.factor, 500 * Appearance.factor)

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
            visible: !unlockPage.closed
            // Asked for its kept passwords only, the primary says where it leads (D-075).
            readonly property string back: VaultController.vaultNameOf(VaultController.returnVault)
            readonly property bool goesBack: VaultController.returnVault !== ""
            textFormat: Text.PlainText
            // The primary asks its master password, any other vault its password (the owner, D-076).
            text: unlockPage.usePin ? qsTr("Enter the PIN to unlock")
                  : goesBack ? qsTr("Enter the Master Password of “%1” to return to “%2”").arg(VaultController.vaultName).arg(back)
                  : unlockPage.primaryShown ? qsTr("Enter the Master Password to unlock") : qsTr("Enter the password to unlock")
            font.pointSize: Appearance.fontPointSize * 1.6
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            Layout.topMargin: 24 * Appearance.factor
            Layout.bottomMargin: 8 * Appearance.factor
        }

        // Every vault Chrono Vault knows, each with its own master password;
        // the one used last is chosen (the owner, D-058).
        VaultBox {
            id: vaultBox
            objectName: "unlockVaultBox"
            // Also with the PIN: another vault picked is a new login (the owner, D-076).
            // Only the vaults found; the one used last also when it is not,
            // struck through (the owner, 0.3.65).
            listed: VaultController.foundVaults.concat(
                VaultController.missingVault !== "" && VaultController.foundVaults.indexOf(VaultController.missingVault) < 0
                    ? [VaultController.missingVault] : [])
            shown: VaultController.vaultPath
            withOther: true
            withLastUsed: true
            Layout.fillWidth: true
            Layout.preferredHeight: 58 * Appearance.factor
            onActivated: function(index) {
                const chosen = model[index]
                // The list follows the vault shown again, whatever was picked.
                currentIndex = Qt.binding(function() { return vaultBox.listed.indexOf(VaultController.vaultPath) })
                if (chosen === other)
                    unlockPage.leaveForAnother(function() { unlockPage.openRequested() })
                else if (chosen !== VaultController.vaultPath && VaultController.keepsChangesOf(chosen))
                    VaultController.switchToVault(chosen) // unlocking it saves its change
                else if (chosen !== VaultController.vaultPath)
                    unlockPage.leaveForAnother(function() { VaultController.switchToVault(chosen) })
                unlockPage.focusPassword()
            }
        }

        BigButton {
            id: openButton
            objectName: "openClosedButton"
            visible: unlockPage.closed
            text: qsTr("Open")
            Layout.fillWidth: true
            onClicked: VaultController.switchToVault(VaultController.vaultPath)
            Keys.onReturnPressed: clicked()
            Keys.onEnterPressed: clicked()
        }
        ExactRow {
            visible: !unlockPage.closed
            gap: 0
            Layout.fillWidth: true
            BigPasswordField {
                id: passwordField
                objectName: "masterPasswordField"
                placeholderText: unlockPage.usePin ? qsTr("PIN") : unlockPage.primaryShown ? qsTr("Master Password") : qsTr("Password")
                Layout.preferredHeight: 58 * Appearance.factor
                Layout.fillWidth: true
                // Greyed while waiting after wrong master passwords (D-041).
                enabled: VaultController.unlockWaitSeconds === 0
                onAccepted: unlockPage.unlockNow()
                onEnabledChanged: if (enabled && unlockPage.visible) forceActiveFocus()
            }
        }
        // As the standard dialog's: kept in the primary, so only while its
        // key store is open (the owner, D-075).
        CheckBox {
            id: rememberBox
            objectName: "unlockRemember"
            visible: VaultController.keyStoreOpen && !unlockPage.closed && !unlockPage.usePin
                     && VaultController.vaultPath !== VaultController.primaryVault
            text: qsTr("Remember my credentials")
            function read() { checked = VaultController.remembersFor(VaultController.vaultPath) }
            onToggled: VaultController.setRemembersFor(VaultController.vaultPath, checked)
        }
        Button {
            objectName: "useMasterPasswordButton"
            visible: unlockPage.usePin
            text: qsTr("Unlock using password instead")
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
            Layout.topMargin: 12 * Appearance.factor
            onClicked: unlockPage.leaveForAnother(function() { unlockPage.createRequested() })
        }
        Label {
            objectName: "unlockWaitText"
            visible: VaultController.unlockWaitSeconds > 0 && !unlockPage.closed
            text: qsTr("Try again in %1").arg(Math.floor(VaultController.unlockWaitSeconds / 60) + ":"
                                              + String(VaultController.unlockWaitSeconds % 60).padStart(2, "0"))
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
        // The keyfile, when this vault needs one (D-031).
        ExactRow {
            visible: VaultController.keyfileNeeded && !unlockPage.closed
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
            visible: VaultController.quickUnlockOn && !VaultController.reminderDue && !unlockPage.closed
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
            visible: VaultController.reminderDue && !unlockPage.closed
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
            visible: VaultController.reminderDue && !unlockPage.closed
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
        fitsContent: true // only asks: as tall as its text (D-079)
        objectName: "leaveVaultDialog"

        memoryName: "leaveVault"
        title: qsTr("A change is not saved")
        standardButtons: Dialog.Discard | Dialog.Cancel
        defaultButton: Dialog.Cancel // Enter never throws a change away
        ExactColumn {
            Label {
                text: qsTr("This vault holds a change that could not be saved yet; unlocking it saves it. If you open another vault now, the change is lost.")
                wrapMode: Text.Wrap
                Layout.preferredWidth: 380 * Appearance.factor
                Layout.fillWidth: true
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
