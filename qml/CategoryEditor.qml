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
// CategoryEditor.qml 0.2.57

// --- Version history ----------------------------------------------------
// v0.2.57: The icon at the left with a pen badge, the whole icon a button; the name beside it (the owner)
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

    readonly property color lineColour: Qt.rgba(palette.text.r, palette.text.g, palette.text.b, 0.14)

    // The icon at the left with a pen in a small circle, the whole icon a
    // button; the name to its right (the owner, as the item editor's).
    ColumnLayout {
        spacing: Math.round(12 * Appearance.factor)
        RowLayout {
            spacing: Math.round(16 * Appearance.factor)
            Item {
                objectName: "categoryIcon"
                Layout.preferredWidth: Math.round(52 * Appearance.factor)
                Layout.preferredHeight: Math.round(52 * Appearance.factor)
                Image {
                    anchors.fill: parent
                    source: categoryEditor.icon !== "" ? "data:image/png;base64," + categoryEditor.icon
                                                       : categoryEditor.defaultIcon
                    sourceSize: Qt.size(width, height)
                    fillMode: Image.PreserveAspectFit
                }
                ToolButton {
                    objectName: "categoryIconBadge"
                    width: Math.round(22 * Appearance.factor)
                    height: width
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: -Math.round(4 * Appearance.factor)
                    icon.source: "qrc:/icons/edit.svg"
                    icon.color: categoryEditor.palette.buttonText
                    icon.width: Math.round(12 * Appearance.factor)
                    icon.height: Math.round(12 * Appearance.factor)
                    background: Rectangle {
                        radius: width / 2
                        color: categoryEditor.palette.window
                        border.color: categoryEditor.lineColour
                    }
                    onClicked: iconPicker.start(categoryEditor.defaultIcon)
                }
                MouseArea {
                    objectName: "categoryIconArea"
                    anchors.fill: parent
                    z: -1 // the badge, on top, answers for itself
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: iconPicker.start(categoryEditor.defaultIcon)
                    ToolTip.visible: containsMouse
                    ToolTip.text: qsTr("Change the icon")
                }
            }
            Label {
                text: qsTr("Name:")
                Layout.alignment: Qt.AlignVCenter
            }
            TextField {
                id: nameField
                objectName: "categoryNameField"
                Layout.preferredWidth: Math.round(280 * Appearance.factor)
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                onAccepted: categoryEditor.accept()
            }
        }
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
