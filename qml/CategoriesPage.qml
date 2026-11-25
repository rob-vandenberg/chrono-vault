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
// CategoriesPage.qml 0.6.88

// --- Version history ----------------------------------------------------
// v0.6.88: Initial release, out of CustomizePage.qml (D-066)

// Settings > Categories (D-027, D-056, D-066): those of the unlocked vault,
// shown or hidden, edited, reset or deleted. Kept in the vault at once.
ColumnLayout {
    id: categoriesPage
    objectName: "categoriesPage"
    spacing: Math.round(8 * Appearance.factor)

    readonly property bool unlocked: VaultController.state === VaultController.Unlocked
    readonly property var categoryList: VaultController.revision >= 0 ? VaultController.categories() : []

    Label {
        text: qsTr("Categories")
        font.pointSize: Appearance.fontPointSize * 1.4
    }
    Label {
        text: categoriesPage.unlocked
              ? qsTr("The categories belong to the vault that is open. Changes here are kept in it at once.")
              : qsTr("Unlock a vault to change its categories.")
        wrapMode: Text.Wrap
        opacity: 0.8
        Layout.fillWidth: true
    }

    Repeater {
        model: categoriesPage.unlocked ? categoriesPage.categoryList : []
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
                objectName: "categoryEdit_" + categoryRow.modelData.id
                text: qsTr("Edit…")
                onClicked: {
                    categoryEditor.active = true
                    categoryEditor.item.edit(categoryRow.modelData)
                }
            }
            // Always on: a Reset of a category already as the program has it
            // changes nothing (the owner).
            Button {
                objectName: "categoryRemove_" + categoryRow.modelData.id
                text: categoryRow.modelData.builtIn ? qsTr("Reset") : qsTr("Delete")
                onClicked: categoryRow.modelData.builtIn ? VaultController.resetCategory(categoryRow.modelData.id)
                                                         : VaultController.deleteCategory(categoryRow.modelData.id)
            }
        }
    }
    Button {
        objectName: "newCategoryButton"
        visible: categoriesPage.unlocked
        text: qsTr("New category…")
        onClicked: categoryDialog.ask("", "")
    }
    Item { Layout.fillHeight: true }

    // Made when first opened, so the window holds no hidden dialog before.
    Loader {
        id: categoryEditor
        active: false
        sourceComponent: CategoryEditor {}
    }

    // A new category of the person's own.
    DialogWindow {
        id: categoryDialog
        objectName: "categoryDialog"
        memoryName: "category"
        property string categoryId: ""
        title: qsTr("New category")
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
