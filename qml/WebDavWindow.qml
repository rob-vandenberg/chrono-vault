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
// WebDavWindow.qml 0.3.63

// --- Version history ----------------------------------------------------
// v0.3.67: Leaving the file dialog without a new choice goes back to the folder of the choice shown
// v0.3.63: One button: Connect, then Open, Next… or Move; the vault or folder chosen with Browse… in a file dialog of
//          its own; the window as tall as its content (the owner, D-057)
// v0.3.62: The server chosen from a list, the folder chosen in a browser with New folder; a reason shown when the
//          window comes back (the owner, D-057)
// v0.3.60: Initial release (D-057)

// A vault on a WebDAV server (D-057): the server (pCloud's two filled in on
// request, any other typed), the account, Connect; then a browser of the
// server's folders, where the vault is made, opened or moved to. A
// certificate the system does not trust is trusted only by its fingerprint;
// a plain http address is warned about.
DialogWindow {
    id: webDavWindow
    objectName: "webDavWindow"
    memoryName: "webDav"

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
    property string serverRoot: ""
    property string chosenFile: ""
    property string chosenFolder: ""
    property bool chosen: false
    property string certificate: ""
    property string trusted: ""
    property string problem: ""
    readonly property int columnWidth: Math.round(460 * Appearance.factor)
    // As tall as its content, whatever height was remembered (the owner).
    maximumHeight: minimumHeight
    readonly property bool busy: connecting || waitingFor !== ""

    // A choice made in the file dialog holds while the folder it was made in
    // is still the one the connection is in.
    readonly property bool ready: connected && !busy && chosen && folderShown === chosenFolder

    // A new vault's name, once a folder is chosen: the master password comes next.
    signal newVaultNamed(string name)

    title: mode === "new" ? qsTr("New vault on a WebDAV server")
         : mode === "open" ? qsTr("Open a vault on a WebDAV server")
         : qsTr("Move the vault to a WebDAV server")
    standardButtons: Dialog.Cancel

    function reset() {
        connecting = false
        connected = false
        waitingFor = ""
        entries = []
        vaultFiles = []
        folderShown = ""
        serverRoot = ""
        chosenFile = ""
        chosenFolder = ""
        chosen = false
        certificate = ""
        trusted = ""
        problem = ""
        passwordField.clear()
    }

    // Shown again with the reason a new vault was not kept (D-057).
    function openWithProblem(reason) {
        open()
        problem = reason
    }

    // The button at the bottom right: Connect until connected, then on to
    // the master password, or open, or move (the owner).
    function go() {
        if (!connected) {
            if (addressField.text.trim() !== "" && !busy)
                connectNow()
            return
        }
        if (!ready)
            return
        if (mode === "new") {
            passwordField.clear()
            accept()
            newVaultNamed(chosenFile)
        } else if (mode === "open") {
            waitingFor = "act"
            VaultController.openWebDavVault(chosenFile)
        } else {
            waitingFor = "act"
            VaultController.moveToWebDav(moveVault)
        }
    }

    function browse() {
        fileDialog.browseFrom(mode === "open" ? "open" : mode === "new" ? "save" : "folder", serverRoot, folderShown,
                        entries, vaultFiles)
    }

    function connectNow() {
        problem = ""
        connected = false
        // Each Connect starts the browser at that server's folder again.
        serverRoot = ""
        folderShown = ""
        entries = []
        vaultFiles = []
        chosenFile = ""
        chosenFolder = ""
        chosen = false
        connecting = true
        VaultController.connectWebDav(addressField.text, userField.text, passwordField.text, trusted)
    }

    onShown: {
        reset()
        serverBox.forceActiveFocus()
    }
    onRejected: passwordField.clear()

    rightButtons: Button {
        objectName: "webDavGoButton"
        text: webDavWindow.connecting ? qsTr("Connecting…") : !webDavWindow.connected ? qsTr("Connect")
            : webDavWindow.mode === "new" ? qsTr("Next…") : webDavWindow.mode === "open" ? qsTr("Open") : qsTr("Move")
        enabled: webDavWindow.connected ? webDavWindow.ready : addressField.text.trim() !== "" && !webDavWindow.busy
        highlighted: true
        onClicked: webDavWindow.go()
    }

    Connections {
        target: VaultController
        function onWebDavConnected(ok, problem, files, certificate) {
            if (!webDavWindow.connecting)
                return
            webDavWindow.connecting = false
            webDavWindow.connected = ok
            webDavWindow.problem = problem
            webDavWindow.certificate = ok ? "" : certificate
        }
        function onWebDavFolderShown(address, entries, files) {
            if (!webDavWindow.visible)
                return
            if (webDavWindow.serverRoot === "")
                webDavWindow.serverRoot = address
            webDavWindow.folderShown = address
            webDavWindow.entries = entries
            webDavWindow.vaultFiles = files
            webDavWindow.waitingFor = ""
        }
        function onWebDavDone(ok, problem) {
            if (webDavWindow.waitingFor === "")
                return
            const acting = webDavWindow.waitingFor === "act"
            webDavWindow.waitingFor = ""
            if (ok && acting) {
                passwordField.clear()
                webDavWindow.accept()
            } else if (!ok) {
                webDavWindow.problem = problem
            }
        }
    }

    ColumnLayout {
        spacing: Math.round(8 * Appearance.factor)

        Label {
            visible: webDavWindow.mode === "move"
            text: qsTr("“%1” goes to the WebDAV folder you choose and stays on this computer as well.").arg(VaultController.vaultNameOf(webDavWindow.moveVault))
            wrapMode: Text.Wrap
            Layout.preferredWidth: webDavWindow.columnWidth
        }
        Label { text: qsTr("Server") }
        // pCloud's addresses are filled in only when chosen, and may be
        // changed: they may change one day (the owner).
        ComboBox {
            id: serverBox
            objectName: "webDavServerBox"
            model: [qsTr("pCloud (Europe)"), qsTr("pCloud (US)"), qsTr("Other WebDAV server")]
            currentIndex: -1
            displayText: currentIndex < 0 ? qsTr("Choose the server…") : currentText
            enabled: !webDavWindow.busy
            Layout.preferredWidth: webDavWindow.columnWidth
            onActivated: function(index) {
                addressField.text = index === 0 ? "https://ewebdav.pcloud.com"
                                  : index === 1 ? "https://webdav.pcloud.com" : ""
                if (index === 2)
                    addressField.forceActiveFocus()
                else
                    userField.forceActiveFocus()
            }
        }
        Label { text: qsTr("Address") }
        TextField {
            id: addressField
            objectName: "webDavAddressField"
            placeholderText: qsTr("https://example.org/remote.php/webdav")
            enabled: !webDavWindow.busy
            Layout.preferredWidth: webDavWindow.columnWidth
            onTextChanged: { webDavWindow.connected = false; webDavWindow.certificate = ""; webDavWindow.trusted = "" }
        }
        Label {
            objectName: "plainHttpWarning"
            visible: VaultController.isPlainHttp(addressField.text)
            text: qsTr("This address is not encrypted (http). The vault itself stays encrypted, but your WebDAV user name and password cross the network readable by anyone on it.")
            color: "#e53e3e"
            wrapMode: Text.Wrap
            Layout.preferredWidth: webDavWindow.columnWidth
        }
        Label { text: qsTr("User name") }
        TextField {
            id: userField
            objectName: "webDavUserField"
            enabled: !webDavWindow.busy
            Layout.preferredWidth: webDavWindow.columnWidth
            onTextChanged: webDavWindow.connected = false
        }
        Label { text: qsTr("Password") }
        PasswordField {
            id: passwordField
            objectName: "webDavPasswordField"
            enabled: !webDavWindow.busy
            Layout.preferredWidth: webDavWindow.columnWidth
            onTextChanged: webDavWindow.connected = false
            onAccepted: webDavWindow.go()
        }
        Label {
            objectName: "webDavProblem"
            visible: webDavWindow.problem !== ""
            text: webDavWindow.problem
            color: "#e53e3e"
            wrapMode: Text.Wrap
            Layout.preferredWidth: webDavWindow.columnWidth
        }

        // A certificate the system does not trust (D-057).
        ColumnLayout {
            visible: webDavWindow.certificate !== ""
            spacing: Math.round(6 * Appearance.factor)
            Label {
                text: qsTr("Trust this certificate only when it is your server's. Its SHA-256 fingerprint:")
                wrapMode: Text.Wrap
                Layout.preferredWidth: webDavWindow.columnWidth
            }
            Label {
                objectName: "webDavFingerprint"
                text: webDavWindow.certificate
                font.family: "Noto Sans Mono"
                wrapMode: Text.WrapAnywhere
                Layout.preferredWidth: webDavWindow.columnWidth
            }
            Button {
                objectName: "trustCertificateButton"
                text: qsTr("Trust this certificate and connect")
                onClicked: {
                    webDavWindow.trusted = webDavWindow.certificate
                    webDavWindow.certificate = ""
                    webDavWindow.connectNow()
                }
            }
        }

        // Once connected: the vault or folder, chosen in the WebDAV file
        // dialog (the owner, D-057).
        Label {
            visible: webDavWindow.connected
            text: webDavWindow.mode === "move" ? qsTr("Folder") : qsTr("Vault")
        }
        RowLayout {
            visible: webDavWindow.connected
            spacing: Math.round(6 * Appearance.factor)
            Layout.preferredWidth: webDavWindow.columnWidth
            TextField {
                objectName: "webDavChosenField"
                readOnly: true
                placeholderText: webDavWindow.mode === "move" ? qsTr("Choose the folder with Browse…")
                               : webDavWindow.mode === "new" ? qsTr("Choose the folder and name with Browse…")
                               : qsTr("Choose the vault with Browse…")
                text: !webDavWindow.chosen ? ""
                      : webDavWindow.chosenFolder.replace(webDavWindow.serverRoot, "/").replace("//", "/")
                        + (webDavWindow.chosenFile === "" ? "" : webDavWindow.chosenFile
                           + (webDavWindow.mode === "new" ? ".chronovault" : ""))
                Layout.fillWidth: true
            }
            Button {
                objectName: "webDavBrowseButton"
                text: qsTr("Browse…")
                enabled: !webDavWindow.busy
                onClicked: webDavWindow.browse()
            }
        }
    }

    WebDavFileDialog {
        id: fileDialog
        transientParent: webDavWindow
        onChosen: function(fileName, folder) {
            webDavWindow.chosenFile = fileName
            webDavWindow.chosenFolder = folder
            webDavWindow.chosen = true
        }
        // Left without a new choice after looking elsewhere: the choice still
        // shown holds, so the connection goes back to its folder.
        onRejected: {
            if (webDavWindow.chosen && webDavWindow.folderShown !== webDavWindow.chosenFolder) {
                webDavWindow.waitingFor = "browse"
                VaultController.openWebDavFolder(webDavWindow.chosenFolder)
            }
        }
    }
}
