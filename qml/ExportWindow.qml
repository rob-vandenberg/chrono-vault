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
// ExportWindow.qml 0.0.22

// --- Version history ----------------------------------------------------
// v0.0.22: Initial release (D-037)

// File > Export… (D-037): a plain warning that the export holds every
// password unencrypted, the master password, and then where to save it.
DialogWindow {
    id: exportWindow
    objectName: "exportWindow"
    memoryName: "export"
    property bool busy: false
    property string problem: ""
    title: qsTr("Export the vault")
    standardButtons: Dialog.Ok | Dialog.Cancel

    function ask() {
        passwordField.clear()
        busy = false
        problem = ""
        open()
        passwordField.forceActiveFocus()
    }

    // Set once the button exists; a binding to it made earlier never took hold.
    Component.onCompleted: {
        exportWindow.standardButton(Dialog.Ok).text = qsTr("Export…")
        exportWindow.standardButton(Dialog.Ok).enabled = Qt.binding(function() { return passwordField.text !== "" && !exportWindow.busy })
    }

    // Export… chooses where; the window stays open until it is written.
    onAccepted: {
        open()
        saveDialog.selectedFile = saveDialog.currentFolder + "/" + encodeURIComponent(VaultController.vaultName + ".json")
        saveDialog.open()
    }
    onRejected: passwordField.clear()

    Connections {
        target: VaultController
        function onExportDone(ok) {
            exportWindow.busy = false
            // Said here too, not only in the main window behind this one.
            exportWindow.problem = ok ? "" : VaultController.message
            if (ok) {
                passwordField.clear()
                exportWindow.answered = true
                exportWindow.hide()
            }
        }
        function onLocking() {
            if (exportWindow.visible) {
                passwordField.clear()
                exportWindow.reject()
            }
        }
    }

    ColumnLayout {
        spacing: Math.round(10 * Appearance.factor)
        Layout.preferredWidth: Math.round(440 * Appearance.factor)
        Label {
            text: qsTr("The export holds every password of “%1” unencrypted: anyone who gets the file can read them all. Keep it safe, and delete it when you no longer need it.").arg(VaultController.vaultName)
            font.bold: true
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("The master password of this vault:")
        }
        PasswordField {
            id: passwordField
            objectName: "exportPasswordField"
            placeholderText: qsTr("Master password")
            Layout.fillWidth: true
            onAccepted: if (text !== "") exportWindow.accept()
        }
        Label {
            objectName: "exportProblem"
            visible: exportWindow.problem !== ""
            text: exportWindow.problem
            color: "#e53e3e"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    FileDialog {
        id: saveDialog
        objectName: "exportSaveDialog"
        parentWindow: exportWindow
        title: qsTr("Where should the export be saved?")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("JSON exports (*.json)")]
        onAccepted: {
            exportWindow.busy = true
            VaultController.exportVault(selectedFile, passwordField.text)
        }
    }
}
