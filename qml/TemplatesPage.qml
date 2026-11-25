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
// TemplatesPage.qml 0.6.88

// --- Version history ----------------------------------------------------
// v0.6.88: Initial release, out of CustomizePage.qml (D-066)

// Settings > Templates (D-027, D-066): those of the unlocked vault; the
// built-in ones can be copied, the person's own also changed and deleted.
// Kept in the vault at once.
ColumnLayout {
    id: templatesPage
    objectName: "templatesPage"
    spacing: Math.round(8 * Appearance.factor)

    readonly property bool unlocked: VaultController.state === VaultController.Unlocked
    readonly property var templateList: VaultController.revision >= 0 ? VaultController.templates() : []

    Label {
        text: qsTr("Templates")
        font.pointSize: Appearance.fontPointSize * 1.4
    }
    Label {
        text: templatesPage.unlocked
              ? qsTr("The templates belong to the vault that is open. Changes here are kept in it at once.")
              : qsTr("Unlock a vault to change its templates.")
        wrapMode: Text.Wrap
        opacity: 0.8
        Layout.fillWidth: true
    }

    Repeater {
        model: templatesPage.unlocked ? templatesPage.templateList : []
        delegate: RowLayout {
            id: templateRow
            required property var modelData
            Layout.fillWidth: true
            spacing: Math.round(6 * Appearance.factor)
            Label {
                text: templateRow.modelData.name
                textFormat: Text.PlainText
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                text: templateRow.modelData.categoryName
                textFormat: Text.PlainText
                opacity: 0.7
            }
            Button {
                text: qsTr("Copy…")
                onClicked: templateEditor.edit(VaultController.copyOfTemplate(templateRow.modelData.id))
            }
            Button {
                visible: !templateRow.modelData.builtIn
                text: qsTr("Edit…")
                onClicked: templateEditor.edit(templateRow.modelData)
            }
            Button {
                visible: !templateRow.modelData.builtIn
                text: qsTr("Delete")
                onClicked: VaultController.deleteTemplate(templateRow.modelData.id)
            }
        }
    }
    Button {
        objectName: "newTemplateButton"
        visible: templatesPage.unlocked
        text: qsTr("New template…")
        onClicked: templateEditor.edit({ id: "", name: "", categoryId: "misc", builtIn: false,
                                         fields: [{ role: "", label: qsTr("New field"), name: qsTr("New field"),
                                                    kind: "text" }] })
    }
    Item { Layout.fillHeight: true }

    TemplateEditor {
        id: templateEditor
        objectName: "templateEditor"
    }
}
