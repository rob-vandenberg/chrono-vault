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
// GoogleDriveWindow.qml 0.8.115

// --- Version history ----------------------------------------------------
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
    readonly property real columnWidth: 460 * Appearance.factor
    // As tall as its content, whatever height was remembered (the owner).
    maximumHeight: minimumHeight
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
    function openWithProblem(reason) {
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

    rightButtons: Button {
        id: goButton
        objectName: "googleDriveGoButton"
        text: driveWindow.connecting ? qsTr("Waiting for Google…") : !driveWindow.connected ? qsTr("Sign in with Google")
            : driveWindow.mode === "new" ? qsTr("Next…") : driveWindow.mode === "open" ? qsTr("Open") : qsTr("Move")
        enabled: driveWindow.connected ? driveWindow.ready : !driveWindow.busy
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
            if (!driveWindow.visible)
                return
            if (driveWindow.driveRoot === "")
                driveWindow.driveRoot = address
            driveWindow.folderShown = address
            driveWindow.entries = entries
            driveWindow.vaultFiles = files
            driveWindow.waitingFor = ""
        }
        // A move answers on its own (D-060).
        function onMoveDone(ok, problem) {
            if (driveWindow.mode !== "move" || driveWindow.waitingFor !== "act")
                return
            driveWindow.waitingFor = ""
            if (ok)
                driveWindow.accept()
            else
                driveWindow.problem = problem
        }
        function onWebDavDone(ok, problem) {
            if (!driveWindow.visible || driveWindow.waitingFor === "")
                return
            const acting = driveWindow.waitingFor === "act"
            driveWindow.waitingFor = ""
            if (ok && acting)
                driveWindow.accept()
            else if (!ok)
                driveWindow.problem = problem
        }
    }

    ExactColumn {
        gap: 8 * Appearance.factor

        Label {
            visible: driveWindow.mode === "move"
            text: qsTr("“%1” moves to the Google Drive folder you choose. Where it is now, it is deleted once its copy there is checked.").arg(VaultController.vaultNameOf(driveWindow.moveVault))
            wrapMode: Text.Wrap
            Layout.preferredWidth: driveWindow.columnWidth
        }
        Label {
            visible: !driveWindow.connected
            text: driveWindow.connecting
                  ? qsTr("Sign in to Google in your web browser, then come back here.")
                  : qsTr("Sign in with Google opens your web browser. Chrono Vault keeps its vaults in the folder “Chrono Vault” in your Google Drive.")
            wrapMode: Text.Wrap
            Layout.preferredWidth: driveWindow.columnWidth
        }
        Label {
            objectName: "googleDriveOnlyOurs"
            visible: driveWindow.connected && driveWindow.mode === "open"
            text: qsTr("Only vaults created or moved here by Chrono Vault are shown. To bring in another vault, open it from this computer and use Move to Google Drive…")
            wrapMode: Text.Wrap
            opacity: 0.7
            Layout.preferredWidth: driveWindow.columnWidth
        }
        Label {
            objectName: "googleDriveProblem"
            visible: driveWindow.problem !== ""
            text: driveWindow.problem
            color: "#e53e3e"
            wrapMode: Text.Wrap
            Layout.preferredWidth: driveWindow.columnWidth
        }

        // Once signed in: the vault or folder, chosen in the file dialog.
        Label {
            visible: driveWindow.connected
            text: driveWindow.mode === "move" ? qsTr("Folder") : qsTr("Vault")
        }
        ExactRow {
            visible: driveWindow.connected
            gap: 6 * Appearance.factor
            Layout.preferredWidth: driveWindow.columnWidth
            TextField {
                objectName: "googleDriveChosenField"
                readOnly: true
                placeholderText: driveWindow.mode === "move" ? qsTr("Choose the folder with Browse…")
                               : driveWindow.mode === "new" ? qsTr("Choose the folder and name with Browse…")
                               : qsTr("Choose the vault with Browse…")
                text: !driveWindow.chosen ? ""
                      : driveWindow.chosenFolder + "/"
                        + (driveWindow.chosenFile === "" ? "" : driveWindow.chosenFile
                           + (driveWindow.mode === "new" ? ".chronovault" : ""))
                Layout.fillWidth: true
            }
            Button {
                objectName: "googleDriveBrowseButton"
                text: qsTr("Browse…")
                enabled: !driveWindow.busy
                onClicked: driveWindow.browse()
            }
        }
    }

    WebDavFileDialog {
        id: fileDialog
        objectName: "googleDriveFileDialog"
        storage: "googledrive"
        transientParent: driveWindow
        onChosen: function(fileName, folder) {
            driveWindow.chosenFile = fileName
            driveWindow.chosenFolder = folder
            driveWindow.chosen = true
        }
        // Left without a new choice after looking elsewhere: back to the
        // folder of the choice shown.
        onRejected: {
            if (driveWindow.chosen && driveWindow.folderShown !== driveWindow.chosenFolder) {
                driveWindow.waitingFor = "browse"
                VaultController.openGoogleDriveFolder(driveWindow.chosenFolder)
            }
        }
    }
}
