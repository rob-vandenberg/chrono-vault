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
// DeleteVaultWindow.qml 0.8.129

// --- Version history ----------------------------------------------------
// v0.8.129: Two dialogs: the password in the standard dialog first, then this warning (the owner, D-083)
// v0.8.123: As wide as the question on one line; whose password it asks; "Password" always; requires its keyfile (D-083)
// v0.8.122: Initial release (D-083)

// Delete… in Settings > Vaults (the owner, D-083): a locked vault's own
// password (and keyfile) is first typed and proven in the standard dialog;
// then this warning, as an extra confirmation. Delete stands apart at the far
// left in red; Cancel is the default; Enter and Esc never delete.
DialogWindow {
    id: deleteWindow
    objectName: "deleteVaultWindow"
    title: qsTr("Delete the vault")

    property string vaultFile: ""
    property string problem: ""
    property bool deleting: false
    readonly property string name: VaultController.vaultNameOf(vaultFile)
    // Just wide enough that the question stands on one line, in every
    // language (the owner, D-083).
    readonly property string question: qsTr("This action cannot be undone. Are you sure you want to delete the vault?")
    TextMetrics {
        id: questionWidth
        font: deleteWindow.font
        text: deleteWindow.question
    }
    fitWidth: Math.max(440, Math.ceil((questionWidth.advanceWidth + 1) / Appearance.factor) + 32)
    readonly property bool busy: deleting

    // The password, proven in the standard dialog, is kept until this
    // warning closes.
    AskWindow {
        id: askWindow
        objectName: "deleteAskWindow"
        transientParent: deleteWindow.transientParent
        typedOnly: true
        action: "delete"
        onRight: {
            handOn()
            deleteWindow.open()
        }
    }
    function askVault(file) {
        problem = ""
        deleting = false
        vaultFile = file
        askWindow.askVault(file)
    }
    function go() {
        if (busy || problem !== "")
            return
        deleting = true
        VaultController.deleteVault(vaultFile)
    }

    standardButtons: Dialog.Cancel
    defaultButton: Dialog.Cancel
    leftButtons: Button {
        objectName: "deleteVaultButton"
        text: qsTr("Delete")
        palette.buttonText: "#ff6b6b" // every button with red text (the owner, D-082)
        // Enter never deletes (the owner, D-083).
        property bool enterNeverPresses: true
        // After a failure the proof is gone: the vault's password again, from its menu.
        enabled: !deleteWindow.busy && deleteWindow.problem === ""
        onClicked: deleteWindow.go()
    }
    // Nothing proven is left behind.
    onVisibleChanged: {
        if (!visible)
            VaultController.stopAsking()
    }
    Connections {
        target: VaultController
        enabled: deleteWindow.visible
        function onDeleteDone(ok, problem) {
            if (!deleteWindow.deleting)
                return
            deleteWindow.deleting = false
            if (ok)
                deleteWindow.accept()
            else
                deleteWindow.problem = problem
        }
    }

    ExactColumn {
        gap: 16 * Appearance.factor
        Layout.fillWidth: true
        ExactColumn {
            gap: 8 * Appearance.factor
            Layout.fillWidth: true
            Label {
                objectName: "deleteWarning"
                text: qsTr("WARNING: You are about to delete the vault “%1”.").arg(deleteWindow.name)
                textFormat: Text.PlainText
                font.bold: true
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                objectName: "dialogText"
                textFormat: Text.RichText
                text: deleteWindow.paragraphs([
                    qsTr("The vault and all information it contains will be permanently lost!"),
                    deleteWindow.question])
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        Label {
            objectName: "deleteVaultProblem"
            visible: text !== ""
            text: deleteWindow.problem
            textFormat: Text.PlainText
            color: "#e53e3e"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }
}
