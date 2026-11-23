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
// WebDavWindow.qml 0.3.60

// --- Version history ----------------------------------------------------
// v0.3.60: Initial release (D-057)

// Connecting to a WebDAV folder (ref: address, user name, password,
// Connect), then: the name of a new vault, the vault to open, or the vault
// to move there. A certificate the system does not trust is shown by its
// fingerprint and trusted only when the person says so; a plain http
// address is warned about (D-057).
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
    property bool busy: false
    property var vaultFiles: []
    property string certificate: ""
    property string trusted: ""
    property string problem: ""
    // The width of the texts, so the window keeps one width however long they are.
    readonly property int columnWidth: Math.round(440 * Appearance.factor)

    readonly property string newFileName: nameField.text.trim() + ".chronovault"
    readonly property bool nameTaken: vaultFiles.some(function(one) { return one.toLowerCase() === newFileName.toLowerCase() })
    // A file name on every system, Windows' rules included (D-057).
    readonly property bool nameGood: nameField.text.trim() !== "" && !/[\/\\:*?"<>|\x00-\x1f]/.test(nameField.text)
                                     && !nameField.text.trim().startsWith(".") && !nameField.text.trim().endsWith(".")
                                     && !/^(con|prn|aux|nul|com[1-9]|lpt[1-9])$/i.test(nameField.text.trim())
    readonly property bool ready: connected && !busy && (mode === "new" ? nameGood && !nameTaken
                                                          : mode === "open" ? fileBox.currentIndex >= 0
                                                          : true)

    // A new vault's name, once connected: the master password comes next.
    signal newVaultNamed(string name)

    title: mode === "new" ? qsTr("New vault on a WebDAV server")
         : mode === "open" ? qsTr("Open a vault on a WebDAV server")
         : qsTr("Move the vault to a WebDAV server")
    standardButtons: Dialog.Cancel

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
            busy = true
            VaultController.openWebDavVault(vaultFiles[fileBox.currentIndex])
        } else {
            busy = true
            VaultController.moveToWebDav(moveVault)
        }
    }

    function connectNow() {
        problem = ""
        connected = false
        connecting = true
        VaultController.connectWebDav(addressField.text, userField.text, passwordField.text, trusted)
    }

    onShown: {
        connecting = false
        connected = false
        busy = false
        vaultFiles = []
        certificate = ""
        trusted = ""
        problem = ""
        passwordField.clear()
        nameField.clear()
        addressField.forceActiveFocus()
    }
    onRejected: passwordField.clear()

    rightButtons: Button {
        objectName: "webDavGoButton"
        text: webDavWindow.mode === "new" ? qsTr("Next…") : webDavWindow.mode === "open" ? qsTr("Open") : qsTr("Move")
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
            webDavWindow.vaultFiles = ok ? files : []
            webDavWindow.problem = problem
            webDavWindow.certificate = ok ? "" : certificate
            if (ok)
                (webDavWindow.mode === "new" ? nameField : fileBox).forceActiveFocus()
        }
        function onWebDavDone(ok, problem) {
            if (!webDavWindow.busy)
                return
            webDavWindow.busy = false
            if (ok) {
                passwordField.clear()
                webDavWindow.accept()
            } else {
                webDavWindow.problem = problem
            }
        }
    }

    ColumnLayout {
        spacing: Math.round(8 * Appearance.factor)

        Label {
            visible: webDavWindow.mode === "move"
            text: qsTr("“%1” goes to the WebDAV folder and stays on this computer as well.").arg(VaultController.vaultNameOf(webDavWindow.moveVault))
            wrapMode: Text.Wrap
            Layout.preferredWidth: webDavWindow.columnWidth
        }
        Label { text: qsTr("WebDAV folder address") }
        TextField {
            id: addressField
            objectName: "webDavAddressField"
            placeholderText: qsTr("https://example.org/remote.php/webdav/Vaults")
            enabled: !webDavWindow.connecting && !webDavWindow.busy
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
            enabled: !webDavWindow.connecting && !webDavWindow.busy
            Layout.preferredWidth: webDavWindow.columnWidth
            onTextChanged: webDavWindow.connected = false
        }
        Label { text: qsTr("Password") }
        PasswordField {
            id: passwordField
            objectName: "webDavPasswordField"
            enabled: !webDavWindow.connecting && !webDavWindow.busy
            Layout.preferredWidth: webDavWindow.columnWidth
            onTextChanged: webDavWindow.connected = false
            onAccepted: if (addressField.text !== "") webDavWindow.connectNow()
        }
        Button {
            objectName: "webDavConnectButton"
            text: webDavWindow.connecting ? qsTr("Connecting…") : qsTr("Connect")
            enabled: addressField.text.trim() !== "" && !webDavWindow.connecting && !webDavWindow.busy
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
            Layout.fillWidth: true
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

        // Connected: what comes next.
        Label {
            visible: webDavWindow.connected
            text: webDavWindow.mode === "open" ? (webDavWindow.vaultFiles.length > 0 ? qsTr("Vault to open") : qsTr("There is no vault in this folder."))
                : webDavWindow.mode === "new" ? qsTr("Name of the new vault")
                : qsTr("Connected. The vault is put in this folder as “%1.chronovault”.").arg(VaultController.vaultNameOf(webDavWindow.moveVault))
            wrapMode: Text.Wrap
            Layout.preferredWidth: webDavWindow.columnWidth
        }
        TextField {
            id: nameField
            objectName: "webDavNameField"
            visible: webDavWindow.connected && webDavWindow.mode === "new"
            Layout.fillWidth: true
            onAccepted: webDavWindow.go()
        }
        Label {
            objectName: "webDavNameProblem"
            visible: nameField.visible && nameField.text.trim() !== "" && !webDavWindow.nameGood
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
        ComboBox {
            id: fileBox
            objectName: "webDavFileBox"
            visible: webDavWindow.connected && webDavWindow.mode === "open" && webDavWindow.vaultFiles.length > 0
            model: webDavWindow.vaultFiles
            Layout.fillWidth: true
        }
    }
}
