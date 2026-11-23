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
// WebDavWindow.qml 0.3.62

// --- Version history ----------------------------------------------------
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
    property var folders: []
    property var vaultFiles: []
    property string folderShown: ""
    property string serverRoot: ""
    property string chosenFile: ""
    property string certificate: ""
    property string trusted: ""
    property string problem: ""
    readonly property int columnWidth: Math.round(460 * Appearance.factor)
    readonly property bool busy: connecting || waitingFor !== ""

    readonly property string newFileName: nameField.text.trim() + ".chronovault"
    readonly property bool nameTaken: vaultFiles.some(function(one) { return one.toLowerCase() === newFileName.toLowerCase() })
    // A file name on every system, Windows' rules included (D-057).
    function goodName(text) {
        const name = text.trim()
        return name !== "" && !/[\/\\:*?"<>|\x00-\x1f]/.test(text) && !name.startsWith(".") && !name.endsWith(".")
                && !/^(con|prn|aux|nul|com[1-9]|lpt[1-9])$/i.test(name)
    }
    readonly property bool ready: connected && !busy && (mode === "new" ? goodName(nameField.text) && !nameTaken
                                                          : mode === "open" ? chosenFile !== ""
                                                          : true)

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
        folders = []
        vaultFiles = []
        folderShown = ""
        serverRoot = ""
        chosenFile = ""
        certificate = ""
        trusted = ""
        problem = ""
        passwordField.clear()
        nameField.clear()
        folderNameField.clear()
        newFolderRow.visible = false
    }

    // Shown again with the reason a new vault was not kept (D-057).
    function openWithProblem(reason) {
        open()
        problem = reason
    }

    // The button at the bottom right: on to the master password, or open, or move.
    function go() {
        if (!ready)
            return
        if (mode === "new") {
            const name = nameField.text.trim()
            passwordField.clear()
            accept()
            newVaultNamed(name)
        } else if (mode === "open") {
            waitingFor = "act"
            VaultController.openWebDavVault(chosenFile)
        } else {
            waitingFor = "act"
            VaultController.moveToWebDav(moveVault)
        }
    }

    function connectNow() {
        problem = ""
        connected = false
        // Each Connect starts the browser at that server's folder again.
        serverRoot = ""
        folderShown = ""
        folders = []
        vaultFiles = []
        chosenFile = ""
        connecting = true
        VaultController.connectWebDav(addressField.text, userField.text, passwordField.text, trusted)
    }

    function openFolder(name) {
        problem = ""
        waitingFor = "browse"
        VaultController.openWebDavFolder(name)
    }

    onShown: {
        reset()
        serverBox.forceActiveFocus()
    }
    onRejected: passwordField.clear()

    rightButtons: Button {
        objectName: "webDavGoButton"
        text: webDavWindow.mode === "new" ? qsTr("Next…") : webDavWindow.mode === "open" ? qsTr("Open") : qsTr("Move here")
        enabled: webDavWindow.ready
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
        function onWebDavFolderShown(address, folders, files) {
            if (!webDavWindow.visible)
                return
            if (webDavWindow.serverRoot === "")
                webDavWindow.serverRoot = address
            webDavWindow.folderShown = address
            webDavWindow.folders = folders
            webDavWindow.vaultFiles = files
            webDavWindow.chosenFile = ""
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
            onAccepted: if (addressField.text !== "") webDavWindow.connectNow()
        }
        Button {
            objectName: "webDavConnectButton"
            text: webDavWindow.connecting ? qsTr("Connecting…") : qsTr("Connect")
            enabled: addressField.text.trim() !== "" && !webDavWindow.busy
            onClicked: webDavWindow.connectNow()
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

        // The folder browser, once connected (the owner, D-057).
        ColumnLayout {
            visible: webDavWindow.connected
            spacing: Math.round(6 * Appearance.factor)
            Label {
                text: webDavWindow.mode === "open" ? qsTr("Choose the vault:")
                    : webDavWindow.mode === "new" ? qsTr("Choose the folder for the new vault:")
                    : qsTr("Choose the folder to move the vault to:")
                font.bold: true
            }
            RowLayout {
                spacing: Math.round(6 * Appearance.factor)
                Layout.preferredWidth: webDavWindow.columnWidth
                Button {
                    objectName: "webDavUpButton"
                    text: qsTr("Up")
                    enabled: !webDavWindow.busy && webDavWindow.folderShown !== webDavWindow.serverRoot
                    onClicked: webDavWindow.openFolder("..")
                }
                Label {
                    objectName: "webDavFolderShown"
                    text: webDavWindow.folderShown.replace(webDavWindow.serverRoot, "/").replace("//", "/")
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
                Button {
                    objectName: "webDavNewFolderButton"
                    text: qsTr("New folder…")
                    enabled: !webDavWindow.busy && webDavWindow.mode !== "open"
                    visible: webDavWindow.mode !== "open"
                    onClicked: {
                        newFolderRow.visible = true
                        folderNameField.forceActiveFocus()
                    }
                }
            }
            RowLayout {
                id: newFolderRow
                visible: false
                spacing: Math.round(6 * Appearance.factor)
                Layout.preferredWidth: webDavWindow.columnWidth
                TextField {
                    id: folderNameField
                    objectName: "webDavFolderNameField"
                    placeholderText: qsTr("Name of the new folder")
                    Layout.fillWidth: true
                    onAccepted: createFolderButton.clicked()
                }
                Button {
                    id: createFolderButton
                    objectName: "webDavCreateFolderButton"
                    text: qsTr("Create")
                    enabled: webDavWindow.goodName(folderNameField.text) && !webDavWindow.busy
                    onClicked: {
                        if (!enabled)
                            return
                        webDavWindow.problem = ""
                        webDavWindow.waitingFor = "browse"
                        VaultController.makeWebDavFolder(folderNameField.text)
                        folderNameField.clear()
                        newFolderRow.visible = false
                    }
                }
            }
            Frame {
                padding: 1
                Layout.preferredWidth: webDavWindow.columnWidth
                Layout.preferredHeight: Math.round(180 * Appearance.factor)
                ListView {
                    id: browser
                    objectName: "webDavBrowser"
                    anchors.fill: parent
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    model: webDavWindow.folders.map(function(name) { return { name: name, folder: true } })
                           .concat(webDavWindow.vaultFiles.map(function(name) { return { name: name, folder: false } }))
                    delegate: ItemDelegate {
                        required property var modelData
                        objectName: (modelData.folder ? "folder_" : "file_") + modelData.name
                        width: ListView.view.width
                        text: (modelData.folder ? "▸  " : "     ") + modelData.name
                        font.bold: modelData.folder
                        enabled: !webDavWindow.busy && (modelData.folder || webDavWindow.mode === "open")
                        highlighted: !modelData.folder && modelData.name === webDavWindow.chosenFile
                        onClicked: {
                            if (modelData.folder)
                                webDavWindow.openFolder(modelData.name)
                            else
                                webDavWindow.chosenFile = modelData.name
                        }
                        onDoubleClicked: if (!modelData.folder) { webDavWindow.chosenFile = modelData.name; webDavWindow.go() }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: browser.count === 0
                        text: qsTr("This folder is empty.")
                        opacity: 0.6
                    }
                }
            }
        }

        Label {
            visible: webDavWindow.connected && webDavWindow.mode === "new"
            text: qsTr("Name of the new vault")
        }
        TextField {
            id: nameField
            objectName: "webDavNameField"
            visible: webDavWindow.connected && webDavWindow.mode === "new"
            Layout.preferredWidth: webDavWindow.columnWidth
            onAccepted: webDavWindow.go()
        }
        Label {
            objectName: "webDavNameProblem"
            visible: nameField.visible && nameField.text.trim() !== "" && !webDavWindow.goodName(nameField.text)
            text: qsTr("A name without / \\ : * ? \" < > |, not starting or ending with a dot.")
            color: "#e53e3e"
            wrapMode: Text.Wrap
            Layout.preferredWidth: webDavWindow.columnWidth
        }
        Label {
            visible: nameField.visible && webDavWindow.nameTaken
            text: qsTr("A vault with that name is already in that folder. Choose another name.")
            color: "#e53e3e"
            wrapMode: Text.Wrap
            Layout.preferredWidth: webDavWindow.columnWidth
        }
    }
}
