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
// ImportWindow.qml 0.6.103

// --- Version history ----------------------------------------------------
// v0.6.103: One standard dialog for a master password, a PIN or a finger (D-073)
// v0.6.92: Exact sizes (D-069)
// v0.5.80: Only a comment (D-055)
// v0.5.78.3: Next… a button of its own: the window stays open and owns the export's file dialog, as Browse… (a debug version)
// v0.5.78.2: Next… for an export goes straight to the file dialog, the window not shown again first (a debug version)
// v0.5.78.1: Next… for an export in the --debug log (a debug version)
// v0.5.78: Texts wrap with the window (D-061)
// v0.5.77: From another vault or from an export; then conflicts to settle (D-060)
// v0.2.51: Its file dialogs as Chrono Viewer's (D-055)
// v0.0.22: OK greyed until ready: set once the button exists, as the earlier binding never took hold
// v0.0.21: The file chooser belongs to the main window, so it shows: this window is still hidden when it opens
// v0.0.19: Initial release (D-035)

// Import… (D-035, D-060), from File > Import… or a vault's menu in Settings >
// Vaults: first where the items come from, another vault (from the list, or any
// vault file with Browse…, with its master password) or an export made
// elsewhere; what it holds is read off the window's thread and summed up;
// only Import adds it to the vault, in one save. Items the vault already has
// are never written over: what differs waits in the Sync conflicts dialog,
// which opens after the import.
DialogWindow {
    id: importWindow
    objectName: "importWindow"
    memoryName: "import"
    readonly property string program: VaultController.referenceName()
    // "choose" first, then "summary".
    property string step: "choose"
    // "vault" or "export".
    property string source: "vault"
    property string vaultFile: ""
    property var summary: ({})
    property bool reading: false
    property string problem: ""
    // Items that differ from the vault's, to be settled after the import.
    signal conflictsToSettle()
    title: qsTr("Import")
    standardButtons: Dialog.Ok | Dialog.Cancel

    // The vaults it can import from: every vault but the one shown.
    readonly property var otherVaults: {
        const all = VaultController.vaults
        const others = []
        for (let i = 0; i < all.length; ++i) {
            if (all[i] !== VaultController.vaultPath)
                others.push(all[i])
        }
        return others
    }

    function start() {
        step = "choose"
        summary = ({})
        reading = false
        problem = ""
        vaultFile = otherVaults.length > 0 ? otherVaults[0] : ""
        open()
    }
    function next() {
        problem = ""
        if (source === "export") {
            // The window stays open and the file dialog belongs to it, as
            // Browse… does.
            FilePickers.logImportStep("Import window stays open", importWindow)
            exportDialog.open()
            return
        }
        if (vaultFile !== "")
            askWindow.askVault(vaultFile)
    }

    // The other vault's master password, or the finger.
    AskWindow {
        id: askWindow
        objectName: "importAskWindow"
        transientParent: importWindow
        title: importWindow.title
        onRight: {
            importWindow.reading = true
            VaultController.importProven()
        }
        onRejected: importWindow.reading = false
    }

    // Next… while choosing: a button of its own, so the window never closes
    // and opens again on the way to the file dialog. Import on the summary;
    // greyed while there is nothing to do. Set once the button exists; a
    // binding made earlier never took hold.
    rightButtons: Button {
        objectName: "importNextButton"
        visible: importWindow.step === "choose"
        text: qsTr("Next…")
        highlighted: true
        enabled: !importWindow.reading
                 && (importWindow.source === "export"
                     || importWindow.vaultFile !== "")
        onClicked: {
            FilePickers.logImportStep("Next… clicked", importWindow)
            importWindow.next()
        }
    }
    Component.onCompleted: {
        importWindow.standardButton(Dialog.Ok).text = qsTr("Import")
        importWindow.standardButton(Dialog.Ok).visible = Qt.binding(function() {
            return importWindow.step === "summary"
        })
        importWindow.standardButton(Dialog.Ok).enabled = Qt.binding(function() {
            return !importWindow.reading && importWindow.step === "summary" && importWindow.summary.ok === true
        })
    }

    Connections {
        target: VaultController
        // The vault locks: what was read is gone, and so is this window.
        function onLocking() {
            if (askWindow.visible)
                askWindow.reject()
            if (importWindow.visible)
                importWindow.reject()
        }
        function onImportRead(result) {
            if (!importWindow.reading)
                return
            importWindow.reading = false
            if (askWindow.visible)
                askWindow.accept()
            importWindow.summary = result
            if (importWindow.source === "vault" && result.ok !== true) {
                importWindow.problem = result.message || ""
                return // the password again, or another vault
            }
            importWindow.step = "summary"
            importWindow.open()
        }
    }

    // OK is Import, on the summary only.
    onAccepted: {
        // Only when this import made conflicts (D-060).
        if (VaultController.confirmImport() && VaultController.lastImportConflicts() > 0)
            conflictsToSettle()
    }
    onRejected: VaultController.cancelImport()

    ExactColumn {
        gap: 10 * Appearance.factor
        Layout.preferredWidth: 460 * Appearance.factor
        Layout.fillWidth: true

        ExactColumn {
            visible: importWindow.step === "choose"
            gap: 6 * Appearance.factor
            Layout.fillWidth: true
            Label {
                text: qsTr("Import into “%1” from:").arg(VaultController.vaultName)
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            RadioButton {
                objectName: "importFromVault"
                text: qsTr("Another vault")
                checked: importWindow.source === "vault"
                onClicked: importWindow.source = "vault"
            }
            ExactRow {
                enabled: importWindow.source === "vault"
                gap: 6 * Appearance.factor
                Layout.fillWidth: true
                Layout.leftMargin: 28 * Appearance.factor
                ComboBox {
                    id: vaultBox
                    objectName: "importVaultBox"
                    Layout.fillWidth: true
                    model: {
                        const names = []
                        const list = importWindow.otherVaults.slice()
                        if (importWindow.vaultFile !== "" && list.indexOf(importWindow.vaultFile) < 0)
                            list.push(importWindow.vaultFile)
                        for (let i = 0; i < list.length; ++i)
                            names.push(VaultController.vaultNameOf(list[i]))
                        return names
                    }
                    currentIndex: {
                        const list = importWindow.otherVaults.slice()
                        if (importWindow.vaultFile !== "" && list.indexOf(importWindow.vaultFile) < 0)
                            list.push(importWindow.vaultFile)
                        return list.indexOf(importWindow.vaultFile)
                    }
                    displayText: currentIndex < 0 ? qsTr("Choose the vault…") : currentText
                    onActivated: function(index) {
                        const list = importWindow.otherVaults.slice()
                        if (importWindow.vaultFile !== "" && list.indexOf(importWindow.vaultFile) < 0)
                            list.push(importWindow.vaultFile)
                        importWindow.vaultFile = list[index]
                    }
                }
                Button {
                    objectName: "importBrowseButton"
                    text: qsTr("Browse…")
                    onClicked: vaultPicker.open()
                }
            }
            RadioButton {
                objectName: "importFromExport"
                text: qsTr("An export of %1").arg(importWindow.program)
                checked: importWindow.source === "export"
                onClicked: importWindow.source = "export"
            }
            Label {
                objectName: "importChooseProblem"
                visible: importWindow.problem !== ""
                text: importWindow.problem
                color: "#e53e3e"
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        Label {
            objectName: "importSummary"
            visible: importWindow.step === "summary" && importWindow.summary.ok === true
            text: qsTr("It holds %1 item(s), %2 attachment(s), %3 category(ies) and %4 template(s) of its own. They are added to “%5”. An item “%5” already has is never written over: when it differs, you choose afterwards.")
                  .arg(importWindow.summary.items || 0).arg(importWindow.summary.attachments || 0)
                  .arg(importWindow.summary.categories || 0).arg(importWindow.summary.templates || 0)
                  .arg(VaultController.vaultName)
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label {
            objectName: "importProblem"
            visible: importWindow.step === "summary" && importWindow.summary.ok === false
            text: importWindow.summary.message || ""
            wrapMode: Text.Wrap
            font.bold: true
            Layout.fillWidth: true
        }
    }

    FilePicker {
        id: exportDialog
        memoryName: "ImportWindow.exportDialog"
        objectName: "exportDialog"
        // Over this window, which stays open, as Browse… is (0.5.78.3).
        parentWindow: importWindow
        title: qsTr("Choose the export of %1").arg(importWindow.program)
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("JSON exports (*.json)"), qsTr("All files (*)")]
        onAccepted: {
            importWindow.reading = true
            VaultController.readImport(selectedFile)
        }
        onRejected: VaultController.cancelImport()
    }

    FilePicker {
        id: vaultPicker
        memoryName: "ImportWindow.vaultPicker"
        parentWindow: importWindow
        title: qsTr("Choose the vault to import")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Chrono Vault vaults (*.chronovault)"), qsTr("All files (*)")]
        onAccepted: importWindow.vaultFile = VaultController.localPath(selectedFile)
    }
}
