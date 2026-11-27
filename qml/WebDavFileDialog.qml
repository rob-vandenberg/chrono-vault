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
// WebDavFileDialog.qml 0.8.120

// --- Version history ----------------------------------------------------
// v0.8.120: Resized by the person; Google Drive's size apart from WebDAV's (D-082)
// v0.8.115: Any storage's folders: WebDAV or Google Drive (D-080)
// v0.6.97: A long text breaks in a window of normal width (D-061)
// v0.6.92: Exact sizes (D-069)
// v0.3.67: A folder joins Back's history only once the next one is shown
// v0.3.63: Initial release (the owner, D-057)

// A file dialog for a WebDAV server (D-057), laid out as Chrono Vault's own
// file dialogs: Back, Up, New folder and the folder's path at the top; the
// folders and vaults with their size and time of change; the file name and
// Open, Save or Choose folder at the bottom.
DialogWindow {
    id: fileDialog
    objectName: "webDavFileDialog"
    // Its own size per storage (D-082).
    memoryName: storage === "googledrive" ? "googleDriveFileDialog" : "webDavFileDialog2"
    // Resized by the person, who finds it at that size again (D-082).
    fitsContent: false

    // "open", "save" or "folder".
    property string mode: "open"
    // The storage whose folders are shown: "webdav" or "googledrive" (D-080).
    property string storage: "webdav"
    property string serverRoot: ""
    property string folderShown: ""
    property var entries: []
    property var vaultFiles: []
    property var history: []
    // The folder being left; it joins the history only once the next one is shown.
    property string leaving: ""
    property bool waiting: false
    property string problem: ""

    // The choice: the file's name ("" for a folder) in the folder shown.
    signal chosen(string fileName, string folder)

    title: mode === "open" ? qsTr("Open a vault") : mode === "save" ? qsTr("Save the new vault") : qsTr("Choose a folder")
    standardButtons: Dialog.Cancel

    readonly property string typedName: {
        const name = nameField.text.trim()
        return mode === "save" && name !== "" && !name.toLowerCase().endsWith(".chronovault") ? name + ".chronovault" : name
    }
    readonly property bool nameTaken: vaultFiles.some(function(one) { return one.toLowerCase() === typedName.toLowerCase() })
    function goodName(text) {
        const name = text.trim()
        return name !== "" && !/[\/\\:*?"<>|\x00-\x1f]/.test(text) && !name.startsWith(".") && !name.endsWith(".")
                && !/^(con|prn|aux|nul|com[1-9]|lpt[1-9])$/i.test(name)
    }
    readonly property bool ready: !waiting && (mode === "folder" ? true
                                               : mode === "open" ? vaultFiles.indexOf(nameField.text) >= 0
                                               : goodName(nameField.text) && !nameTaken)

    function browseFrom(startMode, root, folder, list, files) {
        mode = startMode
        serverRoot = root
        folderShown = folder
        entries = list
        vaultFiles = files
        history = []
        leaving = ""
        problem = ""
        waiting = false
        nameField.clear()
        newFolderRow.visible = false
        open()
    }
    function go(name, remember) {
        leaving = remember ? folderShown : ""
        problem = ""
        waiting = true
        if (storage === "googledrive")
            VaultController.openGoogleDriveFolder(name)
        else
            VaultController.openWebDavFolder(name)
    }
    function back() {
        if (history.length === 0)
            return
        const previous = history[history.length - 1]
        history = history.slice(0, history.length - 1)
        go(previous, false)
    }
    function choose() {
        if (!ready)
            return
        const name = mode === "folder" ? "" : mode === "save" ? typedName.slice(0, -".chronovault".length) : nameField.text
        accept()
        chosen(name, folderShown)
    }

    onShown: (mode === "open" ? browser : nameField).forceActiveFocus()

    rightButtons: Button {
        objectName: "fileDialogChooseButton"
        text: fileDialog.mode === "open" ? qsTr("Open") : fileDialog.mode === "save" ? qsTr("Save") : qsTr("Choose folder")
        enabled: fileDialog.ready
        highlighted: true
        onClicked: fileDialog.choose()
    }

    Connections {
        target: VaultController
        function onWebDavFolderShown(address, entries, files) {
            if (!fileDialog.visible)
                return
            if (fileDialog.leaving !== "" && fileDialog.leaving !== address)
                fileDialog.history = fileDialog.history.concat([fileDialog.leaving])
            fileDialog.leaving = ""
            fileDialog.folderShown = address
            fileDialog.entries = entries
            fileDialog.vaultFiles = files
            fileDialog.waiting = false
            if (fileDialog.mode === "open")
                nameField.clear()
        }
        function onWebDavDone(ok, problem) {
            if (!fileDialog.visible || !fileDialog.waiting)
                return
            fileDialog.waiting = false
            fileDialog.leaving = ""
            if (!ok)
                fileDialog.problem = problem
        }
    }

    ExactColumn {
        gap: 6 * Appearance.factor

        ExactRow {
            gap: 6 * Appearance.factor
            Layout.fillWidth: true
            Button {
                objectName: "fileDialogBackButton"
                text: qsTr("Back")
                enabled: fileDialog.history.length > 0 && !fileDialog.waiting
                onClicked: fileDialog.back()
            }
            Button {
                objectName: "fileDialogUpButton"
                text: qsTr("Up")
                enabled: fileDialog.folderShown !== fileDialog.serverRoot && !fileDialog.waiting
                onClicked: fileDialog.go("..", true)
            }
            TextField {
                objectName: "fileDialogPath"
                // From the server's top, as the folder is shown in the WebDAV window.
                text: fileDialog.storage === "googledrive" ? fileDialog.folderShown
                      : fileDialog.folderShown.replace(fileDialog.serverRoot, "/").replace("//", "/")
                readOnly: true
                Layout.fillWidth: true
            }
            Button {
                objectName: "fileDialogNewFolderButton"
                text: qsTr("New folder…")
                visible: fileDialog.mode !== "open"
                enabled: !fileDialog.waiting
                onClicked: {
                    newFolderRow.visible = true
                    folderNameField.forceActiveFocus()
                }
            }
        }
        ExactRow {
            id: newFolderRow
            visible: false
            gap: 6 * Appearance.factor
            Layout.fillWidth: true
            TextField {
                id: folderNameField
                objectName: "fileDialogFolderNameField"
                placeholderText: qsTr("Name of the new folder")
                Layout.fillWidth: true
                onAccepted: createFolder.clicked()
            }
            Button {
                id: createFolder
                objectName: "fileDialogCreateFolderButton"
                text: qsTr("Create")
                enabled: fileDialog.goodName(folderNameField.text) && !fileDialog.waiting
                onClicked: {
                    if (!enabled)
                        return
                    fileDialog.leaving = fileDialog.folderShown
                    fileDialog.problem = ""
                    fileDialog.waiting = true
                    if (fileDialog.storage === "googledrive")
                        VaultController.makeGoogleDriveFolder(folderNameField.text)
                    else
                        VaultController.makeWebDavFolder(folderNameField.text)
                    folderNameField.clear()
                    newFolderRow.visible = false
                }
            }
        }

        Frame {
            padding: 1
            Layout.fillWidth: true
            Layout.preferredWidth: 600 * Appearance.factor
            Layout.preferredHeight: 300 * Appearance.factor
            ExactColumn {
                anchors.fill: parent
                gap: 0
                // The columns' names.
                ExactRow {
                    gap: 0
                    Layout.fillWidth: true
                    Layout.leftMargin: 8 * Appearance.factor
                    Layout.rightMargin: 8 * Appearance.factor
                    Label { text: qsTr("Name"); font.bold: true; Layout.fillWidth: true }
                    Label { text: qsTr("Size"); font.bold: true; Layout.preferredWidth: 90 * Appearance.factor }
                    Label { text: qsTr("Modified"); font.bold: true; Layout.preferredWidth: 150 * Appearance.factor }
                }
                ListView {
                    id: browser
                    objectName: "fileDialogList"
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ScrollBar.vertical: ScrollBar {}
                    model: fileDialog.entries
                    currentIndex: -1
                    delegate: ItemDelegate {
                        id: row
                        required property var modelData
                        required property int index
                        objectName: (modelData.folder ? "folder_" : "file_") + modelData.name
                        width: ListView.view.width
                        enabled: !fileDialog.waiting && (modelData.folder || fileDialog.mode !== "folder")
                        highlighted: !modelData.folder && modelData.name === nameField.text
                        contentItem: ExactRow {
                            gap: 8 * Appearance.factor
                            Image {
                                source: row.modelData.folder ? "qrc:/icons/folder.svg" : "qrc:/icons/chrono-vault.svg"
                                sourceSize: Qt.size(Math.round(20 * Appearance.factor), Math.round(20 * Appearance.factor))
                                Layout.preferredWidth: Math.round(20 * Appearance.factor)
                                Layout.preferredHeight: Math.round(20 * Appearance.factor)
                            }
                            Label {
                                text: row.modelData.name
                                textFormat: Text.PlainText
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                            Label {
                                text: row.modelData.size
                                opacity: 0.8
                                Layout.preferredWidth: 90 * Appearance.factor
                            }
                            Label {
                                text: row.modelData.modified
                                opacity: 0.8
                                Layout.preferredWidth: 150 * Appearance.factor
                            }
                        }
                        onClicked: if (!modelData.folder) nameField.text = modelData.name
                        onDoubleClicked: {
                            if (modelData.folder)
                                fileDialog.go(modelData.name, true)
                            else {
                                nameField.text = modelData.name
                                fileDialog.choose()
                            }
                        }
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
            objectName: "fileDialogProblem"
            visible: fileDialog.problem !== "" || (fileDialog.mode === "save" && fileDialog.nameTaken)
                     || (fileDialog.mode === "save" && nameField.text.trim() !== "" && !fileDialog.goodName(nameField.text))
            text: fileDialog.problem !== "" ? fileDialog.problem
                : fileDialog.nameTaken ? qsTr("A vault with that name is already in this folder. Choose another name.")
                : qsTr("A name without / \\ : * ? \" < > |, not starting or ending with a dot.")
            color: "#e53e3e"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        ExactRow {
            visible: fileDialog.mode !== "folder"
            gap: 8 * Appearance.factor
            Layout.fillWidth: true
            Label { text: qsTr("File name:") }
            TextField {
                id: nameField
                objectName: "fileDialogNameField"
                readOnly: fileDialog.mode === "open"
                Layout.fillWidth: true
                onAccepted: fileDialog.choose()
            }
        }
    }
}
