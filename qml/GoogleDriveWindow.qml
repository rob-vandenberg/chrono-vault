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
// GoogleDriveWindow.qml 0.8.121

// --- Version history ----------------------------------------------------
// v0.8.121: Once signed in only the file dialog is on screen; a failure shown in its error line (D-082)
// v0.8.120: Signed in: the file dialog opens by itself, a choice goes on at once, its Cancel ends it all; no Vault field (D-082)
// v0.8.115: Initial release: a vault on Google Drive, made, opened or moved there (D-080)

// A vault on Google Drive (D-080): Sign in with Google opens the browser,
// the person signs in at Google, and the "Chrono Vault" folder at the top of
// My Drive is shown; there the vault is made, opened or moved to, in the
// same file dialog as WebDAV's. Only vaults Chrono Vault made or moved there
// are seen (the permission drive.file, the owner, Q15).
DialogWindow {
    id: driveWindow
    objectName: "googleDriveWindow"
    memoryName: "googleDrive"

    // "new", "open" or "move".
    property string mode: "new"
    // The vault to move, with "move".
    property string moveVault: ""

    property bool connecting: false
    property bool connected: false
    // "browse" while a folder is read or made, "act" while a vault is opened or moved.
    property string waitingFor: ""
    property var entries: []
    property var vaultFiles: []
    property string folderShown: ""
    property string driveRoot: ""
    property string chosenFile: ""
    property string chosenFolder: ""
    property bool chosen: false
    property string problem: ""
    // As tall as its content, whatever height was remembered (the owner).
    maximumHeight: minimumHeight
    // Connected or signed in: the file dialog opens by itself, once (D-082).
    property bool browseWhenListed: false
    readonly property bool busy: connecting || waitingFor !== ""
    readonly property bool ready: connected && !busy && chosen && folderShown === chosenFolder

    // A new vault's name, once a folder is chosen: the master password comes next.
    signal newVaultNamed(string name)

    title: mode === "new" ? qsTr("New vault on Google Drive")
         : mode === "open" ? qsTr("Open a vault on Google Drive")
         : qsTr("Move the vault to Google Drive")
    standardButtons: Dialog.Cancel

    function reset() {
        connecting = false
        connected = false
        waitingFor = ""
        entries = []
        vaultFiles = []
        folderShown = ""
        driveRoot = ""
        chosenFile = ""
        chosenFolder = ""
        chosen = false
        problem = ""
    }

    // Shown again with the reason a new vault was not kept.
    // Signed in, it is the file dialog that shows it (the owner, D-082).
    function openWithProblem(reason) {
        if (connected && driveRoot !== "") {
            browse()
            fileDialog.problem = reason
            return
        }
        open()
        problem = reason
    }

    // The button at the bottom right: Sign in until signed in, then on to
    // the master password, or open, or move.
    function go() {
        if (!connected) {
            if (!busy)
                signIn()
            return
        }
        if (!ready)
            return
        if (mode === "new") {
            fileDialog.accept()
            accept()
            newVaultNamed(chosenFile)
        } else if (mode === "open") {
            waitingFor = "act"
            VaultController.openGoogleDriveVault(chosenFile)
        } else {
            waitingFor = "act"
            VaultController.moveVaultToGoogleDrive(moveVault)
        }
    }

    function signIn() {
        problem = ""
        connected = false
        driveRoot = ""
        folderShown = ""
        entries = []
        vaultFiles = []
        chosenFile = ""
        chosenFolder = ""
        chosen = false
        connecting = true
        browseWhenListed = true
        VaultController.connectGoogleDrive()
    }

    function browse() {
        fileDialog.browseFrom(mode === "open" ? "open" : mode === "new" ? "save" : "folder", driveRoot, folderShown,
                              entries, vaultFiles)
    }

    onShown: {
        reset()
        goButton.forceActiveFocus()
    }
    onRejected: {
        if (connecting)
            VaultController.cancelGoogleSignIn()
        connecting = false
    }

    // Shown only while sign-in is needed: once signed in this window closes
    // and only the file dialog is on screen (the owner, D-082).
    rightButtons: Button {
        id: goButton
        objectName: "googleDriveGoButton"
        text: driveWindow.connecting ? qsTr("Waiting for Google…") : qsTr("Sign in with Google")
        enabled: !driveWindow.busy
        highlighted: true
        onClicked: driveWindow.go()
    }

    Connections {
        target: VaultController
        function onGoogleDriveConnected(ok, problem) {
            if (!driveWindow.connecting)
                return
            driveWindow.connecting = false
            driveWindow.connected = ok
            driveWindow.problem = problem
        }
        function onWebDavFolderShown(address, entries, files) {
            if (!driveWindow.visible && !fileDialog.visible)
                return
            if (driveWindow.driveRoot === "")
                driveWindow.driveRoot = address
            driveWindow.folderShown = address
            driveWindow.entries = entries
            driveWindow.vaultFiles = files
            driveWindow.waitingFor = ""
            if (driveWindow.browseWhenListed) {
                driveWindow.browseWhenListed = false
                // Signed in: this window goes, the file dialog comes.
                driveWindow.hide()
                driveWindow.browse()
            }
        }
        // A move answers on its own (D-060).
        function onMoveDone(ok, problem) {
            if (driveWindow.mode !== "move" || driveWindow.waitingFor !== "act")
                return
            driveWindow.waitingFor = ""
            if (ok) {
                fileDialog.accept()
                driveWindow.accept()
            } else {
                fileDialog.waiting = false
                fileDialog.problem = problem
            }
        }
        // A failure is shown by the file dialog itself, which waits for it.
        function onWebDavDone(ok, problem) {
            if ((!driveWindow.visible && !fileDialog.visible) || driveWindow.waitingFor === "")
                return
            const acting = driveWindow.waitingFor === "act"
            driveWindow.waitingFor = ""
            if (ok && acting) {
                fileDialog.accept()
                driveWindow.accept()
            } else if (!ok) {
                driveWindow.problem = problem
            }
        }
    }

    ExactColumn {
        gap: 8 * Appearance.factor

        Label {
            visible: driveWindow.mode === "move"
            text: qsTr("“%1” moves to the Google Drive folder you choose. Where it is now, it is deleted once its copy there is checked.").arg(VaultController.vaultNameOf(driveWindow.moveVault))
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label {
            visible: !driveWindow.connected
            text: driveWindow.connecting
                  ? qsTr("Sign in to Google in your web browser, then come back here.")
                  : qsTr("Sign in with Google opens your web browser. Chrono Vault keeps its vaults in the folder “Chrono Vault” in your Google Drive.")
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label {
            objectName: "googleDriveProblem"
            visible: driveWindow.problem !== ""
            text: driveWindow.problem
            color: "#e53e3e"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    WebDavFileDialog {
        id: fileDialog
        objectName: "googleDriveFileDialog"
        storage: "googledrive"
        actsBeforeClosing: true
        // Over the main window: the sign-in window is gone by then.
        transientParent: driveWindow.transientParent
        // Chosen: on at once, a new vault to its password (D-082).
        onChosen: function(fileName, folder) {
            driveWindow.chosenFile = fileName
            driveWindow.chosenFolder = folder
            driveWindow.chosen = true
            Qt.callLater(driveWindow.go)
        }
        // Cancel in the file dialog ends it all: it is the only window seen
        // (the owner, D-082).
        onRejected: driveWindow.reject()
    }
}
