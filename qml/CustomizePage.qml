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
// CustomizePage.qml 0.0.14

// --- Version history ----------------------------------------------------
// v0.0.14: Initial release (D-027)

// Settings > Customize (D-027): the categories and templates of the unlocked
// vault. The built-in ones can be hidden or copied; the person's own can also
// be renamed, changed and deleted. Kept in the vault at once, not with OK.
ColumnLayout {
    id: customizePage
    objectName: "customizePage"
    spacing: Math.round(8 * Appearance.factor)

    readonly property bool unlocked: VaultController.state === VaultController.Unlocked
    readonly property var categoryList: VaultController.revision >= 0 ? VaultController.categories() : []
    readonly property var templateList: VaultController.revision >= 0 ? VaultController.templates() : []

    Label {
        text: qsTr("Customize")
        font.pointSize: Appearance.fontPointSize * 1.4
    }
    Label {
        visible: !customizePage.unlocked
        text: qsTr("Unlock a vault to change its categories and templates.")
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    Label {
        visible: customizePage.unlocked
        text: qsTr("The categories and templates belong to the vault that is open. Changes here are kept in it at once.")
        wrapMode: Text.Wrap
        opacity: 0.8
        Layout.fillWidth: true
    }

    Label {
        visible: customizePage.unlocked
        text: qsTr("Categories")
        font.bold: true
        Layout.topMargin: Math.round(8 * Appearance.factor)
    }
    Repeater {
        model: customizePage.unlocked ? customizePage.categoryList : []
        delegate: RowLayout {
            id: categoryRow
            required property var modelData
            Layout.fillWidth: true
            spacing: Math.round(6 * Appearance.factor)
            Image {
                source: categoryRow.modelData.icon
                sourceSize: Qt.size(Math.round(20 * Appearance.factor), Math.round(20 * Appearance.factor))
            }
            CheckBox {
                objectName: "categoryShown_" + categoryRow.modelData.id
                text: categoryRow.modelData.name
                checked: !categoryRow.modelData.hidden
                onToggled: VaultController.setCategoryHidden(categoryRow.modelData.id, !checked)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Shown in the left column")
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("%n item(s)", "", categoryRow.modelData.count)
                opacity: 0.7
            }
            Button {
                visible: !categoryRow.modelData.builtIn
                text: qsTr("Rename…")
                onClicked: categoryDialog.ask(categoryRow.modelData.id, categoryRow.modelData.name)
            }
            Button {
                visible: !categoryRow.modelData.builtIn
                text: qsTr("Delete")
                onClicked: VaultController.deleteCategory(categoryRow.modelData.id)
            }
        }
    }
    Button {
        objectName: "newCategoryButton"
        visible: customizePage.unlocked
        text: qsTr("New category…")
        onClicked: categoryDialog.ask("", "")
    }

    Label {
        visible: customizePage.unlocked
        text: qsTr("Templates")
        font.bold: true
        Layout.topMargin: Math.round(12 * Appearance.factor)
    }
    Repeater {
        model: customizePage.unlocked ? customizePage.templateList : []
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
        visible: customizePage.unlocked
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

    // A category of the person's own: new, or renamed.
    DialogWindow {
        id: categoryDialog
        objectName: "categoryDialog"
        memoryName: "category"
        property string categoryId: ""
        title: categoryId === "" ? qsTr("New category") : qsTr("Rename category")
        standardButtons: Dialog.Ok | Dialog.Cancel
        function ask(id, name) {
            categoryId = id
            categoryName.text = name
            open()
            categoryName.forceActiveFocus()
            categoryName.selectAll()
        }
        ColumnLayout {
            Label {
                text: qsTr("Name of the category:")
            }
            TextField {
                id: categoryName
                objectName: "categoryNameField"
                Layout.preferredWidth: Math.round(320 * Appearance.factor)
                Layout.fillWidth: true
                onAccepted: categoryDialog.accept()
            }
        }
        onAccepted: VaultController.saveCategory(categoryId, categoryName.text)
    }
}
