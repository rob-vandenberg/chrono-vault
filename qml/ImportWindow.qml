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
// ImportWindow.qml 0.0.22

// --- Version history ----------------------------------------------------
// v0.0.22: OK greyed until ready: set once the button exists, as the earlier binding never took hold
// v0.0.21: The file chooser belongs to the main window, so it shows: this window is still hidden when it opens
// v0.0.19: Initial release (D-035)

// File > Import… (D-035): the person picks the reference program's JSON
// export; what it holds is read off the window's thread and summed up here;
// only Import adds it to the vault, in one save.
DialogWindow {
    id: importWindow
    objectName: "importWindow"
    memoryName: "import"
    readonly property string program: VaultController.referenceName()
    property var summary: ({})
    property bool reading: false
    title: qsTr("Import from %1").arg(program)
    standardButtons: Dialog.Ok | Dialog.Cancel

    function start() {
        summary = ({})
        reading = false
        exportDialog.open()
    }

    // Import, greyed while there is nothing to import.
    // Set once the button exists; a binding to it made earlier never took hold.
    Component.onCompleted: {
        importWindow.standardButton(Dialog.Ok).text = qsTr("Import")
        importWindow.standardButton(Dialog.Ok).enabled = Qt.binding(function() { return importWindow.summary.ok === true })
    }

    Connections {
        target: VaultController
        // The vault locks: what was read is gone, and so is this window.
        function onLocking() {
            if (importWindow.visible)
                importWindow.reject()
        }
        function onImportRead(result) {
            if (!importWindow.reading)
                return
            importWindow.reading = false
            importWindow.summary = result
            importWindow.open()
        }
    }

    onAccepted: VaultController.confirmImport()
    onRejected: VaultController.cancelImport()

    ColumnLayout {
        spacing: Math.round(10 * Appearance.factor)
        Layout.preferredWidth: Math.round(420 * Appearance.factor)
        Label {
            objectName: "importSummary"
            visible: importWindow.summary.ok === true
            text: qsTr("The export holds %1 item(s), %2 attachment(s), %3 category(ies) and %4 template(s) of your own. They are added to “%5”, as they are.")
                  .arg(importWindow.summary.items || 0).arg(importWindow.summary.attachments || 0)
                  .arg(importWindow.summary.categories || 0).arg(importWindow.summary.templates || 0)
                  .arg(VaultController.vaultName)
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label {
            objectName: "importProblem"
            visible: importWindow.summary.ok === false
            text: importWindow.summary.message || ""
            wrapMode: Text.Wrap
            font.bold: true
            Layout.fillWidth: true
        }
    }

    FileDialog {
        id: exportDialog
        objectName: "exportDialog"
        // This window is still hidden when the chooser opens: it belongs to
        // the main window, or it would not be shown.
        parentWindow: importWindow.transientParent
        title: qsTr("Choose the export of %1").arg(importWindow.program)
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("JSON exports (*.json)"), qsTr("All files (*)")]
        onAccepted: {
            importWindow.reading = true
            VaultController.readImport(selectedFile)
        }
    }
}
