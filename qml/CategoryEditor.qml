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
// CategoryEditor.qml 0.2.56

// --- Version history ----------------------------------------------------
// v0.2.56: Initial release (D-056)

// Edit a category, built-in or the person's own (D-056): its name, its icon
// and whether it is shown in the left column. Opened from Settings >
// Customize and from a right-click on a category in the left column.
DialogWindow {
    id: categoryEditor
    objectName: "categoryEditor"
    memoryName: "categoryEditor"
    title: qsTr("Edit category")
    standardButtons: Dialog.Ok | Dialog.Cancel

    property string categoryId: ""
    // The icon to keep: PNG in base64, or "" for the program's own.
    property string icon: ""
    property url defaultIcon

    // category: a row of VaultController.categories().
    function edit(category) {
        categoryId = category.id
        nameField.text = category.name
        icon = category.ownIcon
        defaultIcon = category.defaultIcon
        shownBox.checked = !category.hidden
        open()
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    GridLayout {
        columns: 2
        columnSpacing: Math.round(12 * Appearance.factor)
        rowSpacing: Math.round(8 * Appearance.factor)

        Label { text: qsTr("Name:") }
        TextField {
            id: nameField
            objectName: "categoryNameField"
            Layout.preferredWidth: Math.round(280 * Appearance.factor)
            Layout.fillWidth: true
            onAccepted: categoryEditor.accept()
        }

        Label { text: qsTr("Icon:") }
        // A click on the icon opens the icon picker, as in the item editor.
        AbstractButton {
            id: iconButton
            objectName: "categoryIconButton"
            implicitWidth: Math.round(40 * Appearance.factor)
            implicitHeight: implicitWidth
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Choose an icon")
            contentItem: Image {
                source: categoryEditor.icon !== "" ? "data:image/png;base64," + categoryEditor.icon
                                                   : categoryEditor.defaultIcon
                sourceSize: Qt.size(width, height)
                fillMode: Image.PreserveAspectFit
            }
            onClicked: iconPicker.start(categoryEditor.defaultIcon)
        }

        Item { width: 1; height: 1 }
        CheckBox {
            id: shownBox
            objectName: "categoryShownBox"
            text: qsTr("Shown in the left column")
        }
    }

    IconPicker {
        id: iconPicker
        objectName: "categoryIconPicker"
        onIconChosen: function(chosen) { categoryEditor.icon = chosen }
        onIconDeleted: function(deleted) {
            if (deleted === categoryEditor.icon)
                categoryEditor.icon = ""
        }
    }

    // Not kept (a name missing or taken): the dialog stays, the message says why.
    onAccepted: {
        if (!VaultController.editCategory(categoryId, nameField.text, icon, shownBox.checked))
            open()
    }
}
