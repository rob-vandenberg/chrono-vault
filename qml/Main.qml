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
// Main.qml 0.0.5

// --- Version history ----------------------------------------------------
// v0.0.5: F1 opens About; the Update window; a Quit for an update waits for the editor or a save and goes on
// v0.0.4: The version in the title; hiding to the tray, Quit and Ctrl+Q
// v0.0.3: The start, unlock and vault pages, the tool bar, the message line with Try saving again
// v0.0.1: Initial release

ApplicationWindow {
    id: mainWindow

    width: 960
    height: 640
    // Shown by the program as it starts: normally, minimized, or not at all
    // when "Start Chrono Vault minimized" keeps it in the tray (D-019).
    visible: false
    title: VaultController.state === VaultController.Unlocked
           ? qsTr("%1 – Chrono Vault %2").arg(VaultController.vaultName).arg(VaultController.version)
           : qsTr("Chrono Vault %1").arg(VaultController.version)

    readonly property bool busy: VaultController.state === VaultController.Busy

    header: ToolBar {
        visible: VaultController.state === VaultController.Unlocked
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            ToolButton {
                text: qsTr("New login")
                onClicked: vaultPage.newLogin()
            }
            ToolButton {
                text: qsTr("New secure note")
                onClicked: vaultPage.newSecureNote()
            }
            Item { Layout.fillWidth: true }
            ToolButton {
                text: qsTr("Settings")
                onClicked: settingsWindow.showWindow()
            }
            ToolButton {
                text: qsTr("Lock")
                onClicked: VaultController.lock()
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        enabled: !mainWindow.busy
        currentIndex: VaultController.state === VaultController.Unlocked ? 2
                      : (VaultController.vaultPath === "" ? 0 : 1)

        WelcomePage {}
        UnlockPage {}
        VaultPage { id: vaultPage }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: mainWindow.busy
        visible: running
    }

    // Quit, from the tray or with Ctrl+Q: asks about unsaved work like every
    // close (D-016), then ends the program.
    property bool quitting: false
    function requestQuit() {
        quitting = true
        if (!visible)
            show()
        close()
    }

    Shortcut {
        sequences: [StandardKey.Quit]
        context: Qt.ApplicationShortcut
        onActivated: mainWindow.requestQuit()
    }

    // Closing never drops work without asking (OR-15).
    property bool quitAnyway: false
    property bool closeWhenSaved: false
    property bool closeWhenSavedQuits: false
    // The quit dialog stands in the way of a Quit (not of a close to the tray).
    property bool quitDialogFromQuit: false
    onClosing: function(close) {
        // Logging out or shutting down: never hold it up. What is being typed is
        // stored, and a running save finishes before the program ends (OR-15).
        if (VaultController.isSessionEnding()) {
            if (vaultPage.hasUnsavedEdit())
                vaultPage.storeEdit()
            settingsWindow.close()
            Qt.quit()
            return
        }
        // Closing the window keeps Chrono Vault running in the tray (D-019).
        if (!quitting && !quitAnyway && VaultController.keepInTray && VaultController.trayAvailable) {
            close.accepted = false
            settingsWindow.close()
            hide()
            return
        }
        if (!quitAnyway) {
            // A close that is held up leaves no "quitting" mark behind: a Quit
            // that is cancelled must not turn the next close into a Quit. What
            // follows remembers whether it was one.
            const wasQuit = quitting
            quitting = false
            // A Quit that waits for the editor or a running save goes on by
            // itself afterwards, so an update stays armed; only a Quit the
            // person abandons (Cancel in the editor's box, Retry or Cancel in
            // the quit dialog) disarms it.
            if (vaultPage.hasUnsavedEdit()) {
                close.accepted = false
                vaultPage.whenEditorFree(function() { wasQuit ? mainWindow.requestQuit() : mainWindow.close() },
                                         function() { if (wasQuit) Updater.quitHeldUp() })
                return
            }
            if (VaultController.saveFailed || VaultController.hasKeptChanges) {
                close.accepted = false
                quitDialogFromQuit = wasQuit
                quitDialog.open()
                return
            }
            if (VaultController.unsavedChanges) {
                // A save is still running: close once it has finished.
                close.accepted = false
                closeWhenSaved = true
                closeWhenSavedQuits = wasQuit
                return
            }
        }
        // The close is accepted: the program ends (it never ends by itself when
        // its last window goes, D-019).
        settingsWindow.close()
        Qt.quit()
    }

    Connections {
        target: VaultController
        function onUnsavedChangesChanged() {
            if (mainWindow.closeWhenSaved && !VaultController.unsavedChanges) {
                mainWindow.closeWhenSaved = false
                mainWindow.closeWhenSavedQuits ? mainWindow.requestQuit() : mainWindow.close()
            }
        }
        function onSaveFailedChanged() {
            if (mainWindow.closeWhenSaved && VaultController.saveFailed) {
                mainWindow.closeWhenSaved = false
                mainWindow.quitDialogFromQuit = mainWindow.closeWhenSavedQuits
                quitDialog.open()
            }
        }
    }

    DialogWindow {

        id: quitDialog

        memoryName: "quit"
        objectName: "quitDialog"
        title: qsTr("A change is not saved")
        standardButtons: VaultController.state === VaultController.Unlocked
                         ? Dialog.Retry | Dialog.Discard | Dialog.Cancel
                         : Dialog.Discard | Dialog.Cancel
        ColumnLayout {
            Label {
                text: VaultController.state === VaultController.Unlocked
                      ? qsTr("Your last change could not be saved. If you quit now, it is lost. The vault on disk is unchanged.")
                      : qsTr("This vault holds a change that could not be saved yet; unlocking it saves it. If you quit now, it is lost.")
                wrapMode: Text.Wrap
                Layout.preferredWidth: 380
            }
        }
        // Retry or Cancel abandon the Quit: an update armed by it is not set
        // off at a later, ordinary quit. Discard goes on with the Quit.
        function quitAbandoned() {
            if (mainWindow.quitDialogFromQuit)
                Updater.quitHeldUp()
            mainWindow.quitDialogFromQuit = false
        }
        onAccepted: {
            quitAbandoned()
            VaultController.retrySave()
        }
        onRejected: quitAbandoned()
        onDiscarded: {
            quitDialog.close()
            mainWindow.quitAnyway = true
            mainWindow.close()
        }
    }

    footer: ColumnLayout {
        spacing: 0
        visible: Updater.state === Updater.Available
                 || VaultController.message !== ""
                 || (VaultController.saveFailed && VaultController.state === VaultController.Unlocked)
        RowLayout {
            visible: Updater.state === Updater.Available
            Layout.fillWidth: true
            Label {
                text: Updater.message
                wrapMode: Text.Wrap
                padding: 8
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Update…")
                onClicked: updateWindow.open()
                Layout.rightMargin: 8
            }
        }
        Label {
            visible: VaultController.message !== ""
            text: VaultController.message
            font.bold: VaultController.messageIsTrouble
            wrapMode: Text.Wrap
            padding: 8
            Layout.fillWidth: true
        }
        RowLayout {
            visible: VaultController.saveFailed && VaultController.state === VaultController.Unlocked
            Layout.fillWidth: true
            Label {
                text: qsTr("A change is not saved yet. The vault on disk is unchanged.")
                font.bold: true
                wrapMode: Text.Wrap
                padding: 8
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Try saving again")
                onClicked: VaultController.retrySave()
                Layout.rightMargin: 8
            }
        }
    }

    SettingsWindow { id: settingsWindow }

    // About, with F1 for now (no menu item yet), and the update (D-021).
    AboutWindow {
        id: aboutWindow
        onCheckRequested: {
            Updater.checkNow()
            updateWindow.open()
        }
    }
    UpdateWindow { id: updateWindow }
    Shortcut {
        sequences: ["F1"]
        context: Qt.ApplicationShortcut
        onActivated: aboutWindow.open()
    }
    Connections {
        target: Updater
        function onQuitRequested() { mainWindow.requestQuit() }
    }

    WindowMemory {
        window: mainWindow
        name: "main"
    }
}
