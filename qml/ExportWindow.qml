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
// ExportWindow.qml 0.8.120

// --- Version history ----------------------------------------------------
// v0.8.120: Its text in normal weight, the one width (D-082)
// v0.7.111: Question dialogs fit their text (D-079)
// v0.6.103: One standard dialog for a master password, a PIN or a finger (D-073)
// v0.6.92: Exact sizes (D-069)
// v0.5.78: Texts wrap with the window (D-061)
// v0.2.51: Its file dialogs as Chrono Viewer's (D-055)
// v0.0.22: Initial release (D-037)

// File > Export… (D-037): a plain warning that the export holds every
// password unencrypted, the master password, and then where to save it.
DialogWindow {
    id: exportWindow
    fitsContent: true // only asks: as tall as its text (D-079)
    objectName: "exportWindow"
    memoryName: "export"
    property bool busy: false
    property string problem: ""
    title: qsTr("Export the vault")
    standardButtons: Dialog.Ok | Dialog.Cancel

    function ask() {
        busy = false
        problem = ""
        open()
    }

    // Set once the button exists; a binding to it made earlier never took hold.
    Component.onCompleted: {
        exportWindow.standardButton(Dialog.Ok).text = qsTr("Export…")
        exportWindow.standardButton(Dialog.Ok).enabled = Qt.binding(function() { return !exportWindow.busy })
    }

    // Export… asks for the master password, or the finger, and then where; the
    // window stays open until it is written.
    onAccepted: {
        open()
        problem = ""
        askWindow.askVault(VaultController.vaultPath)
    }

    Connections {
        target: VaultController
        function onExportDone(ok) {
            exportWindow.busy = false
            if (askWindow.visible)
                askWindow.accept()
            // Said here too, not only in the main window behind this one.
            exportWindow.problem = ok ? "" : VaultController.message
            if (ok) {
                exportWindow.answered = true
                exportWindow.hide()
            }
        }
        function onLocking() {
            if (askWindow.visible)
                askWindow.reject()
            if (exportWindow.visible)
                exportWindow.reject()
        }
    }

    AskWindow {
        id: askWindow
        objectName: "exportAskWindow"
        transientParent: exportWindow
        title: exportWindow.title
        onRight: {
            saveDialog.selectedFile = saveDialog.currentFolder + "/" + encodeURIComponent(VaultController.vaultName + ".json")
            saveDialog.open()
        }
    }

    ExactColumn {
        gap: 10 * Appearance.factor
        Layout.fillWidth: true
        Label {
            text: qsTr("The export holds every password of “%1” unencrypted: anyone who gets the file can read them all. Keep it safe, and delete it when you no longer need it.").arg(VaultController.vaultName)
            wrapMode: Text.Wrap
            Layout.fillWidth: true
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

    FilePicker {
        id: saveDialog
        memoryName: "ExportWindow.saveDialog"
        objectName: "exportSaveDialog"
        parentWindow: askWindow
        title: qsTr("Where should the export be saved?")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("JSON exports (*.json)")]
        onAccepted: {
            exportWindow.busy = true
            VaultController.exportProven(selectedFile)
        }
        onRejected: askWindow.reject()
    }
}
