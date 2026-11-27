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
// TemplateEditor.qml 0.8.121

// --- Version history ----------------------------------------------------
// v0.8.121: The list of fields grows, Add a field right under it (D-082)
// v0.8.120: Resized by the person, its own size kept (D-082)
// v0.6.100: Padlocks and Open in Settings > Vaults; round picture buttons (D-071)
// v0.6.92: Exact sizes (D-069)
// v0.5.78: Its content follows the window's width (D-061)
// v0.0.14: Initial release (D-027)

// A template of the person's own (D-027): its name, its category, and its
// fields, each with a kind, added, removed and put in order. Items made from
// it before are not changed.
DialogWindow {
    id: templateEditor
    memoryName: "templateEditor"
    // Resized by the person, who finds it at that size again (D-082).
    fitsContent: false
    title: templateId === "" ? qsTr("New template") : qsTr("Edit template")
    standardButtons: Dialog.Save | Dialog.Cancel

    property string templateId: ""
    readonly property var kinds: [
        { kind: "text", name: qsTr("Text") },
        { kind: "password", name: qsTr("Password") },
        { kind: "multiline", name: qsTr("Several lines") },
        { kind: "url", name: qsTr("Web address") },
        { kind: "totp", name: qsTr("One-time code") }
    ]
    readonly property var categoryList: VaultController.revision >= 0 ? VaultController.categories() : []

    function kindIndex(kind) {
        for (let index = 0; index < kinds.length; ++index) {
            if (kinds[index].kind === kind)
                return index
        }
        return 0
    }

    function edit(map) {
        templateId = map.id
        nameField.text = map.name
        let chosen = 0
        for (let index = 0; index < categoryList.length; ++index) {
            if (categoryList[index].id === map.categoryId)
                chosen = index
        }
        categoryBox.currentIndex = chosen
        fieldsModel.clear()
        for (let field of map.fields)
            fieldsModel.append({ role: field.role, label: field.label, name: field.name, kind: field.kind })
        open()
        nameField.forceActiveFocus()
    }

    function moveField(index, step) {
        const target = index + step
        if (target >= 0 && target < fieldsModel.count)
            fieldsModel.move(index, target, 1)
    }

    function save() {
        let fields = []
        for (let index = 0; index < fieldsModel.count; ++index) {
            const field = fieldsModel.get(index)
            fields.push({ role: field.role, label: field.label, kind: field.kind })
        }
        const category = categoryList[categoryBox.currentIndex]
        return VaultController.saveTemplate({ id: templateId, name: nameField.text,
                                               categoryId: category ? category.id : "misc", fields: fields })
    }

    // Kept, or shown again when it could not be (no name): nothing typed is lost.
    onAccepted: {
        if (save() === "")
            open()
    }

    ListModel { id: fieldsModel }

    // Name and Category at the top; the list of fields takes all extra room,
    // "Add a field" right under it (the owner, D-082).
    ExactColumn {
        gap: 16 * Appearance.factor
        Layout.preferredWidth: 520 * Appearance.factor
        Layout.fillWidth: true
        Layout.fillHeight: true

        GridLayout {
            columns: 2
            columnSpacing: 12 * Appearance.factor
            rowSpacing: 8 * Appearance.factor
            Layout.fillWidth: true
            Label { text: qsTr("Name:") }
            TextField {
                id: nameField
                objectName: "templateNameField"
                Layout.fillWidth: true
            }
            Label { text: qsTr("Category:") }
            ComboBox {
                id: categoryBox
                objectName: "templateCategoryBox"
                model: templateEditor.categoryList
                textRole: "name"
                Layout.fillWidth: true
            }
        }

        ExactColumn {
        gap: 8 * Appearance.factor
        Layout.fillWidth: true
        Layout.fillHeight: true
        Label {
            text: qsTr("Fields")
            font.bold: true
        }
        ScrollView {
            id: fieldScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Opens as tall as its fields, within reason (D-082).
            Layout.preferredHeight: Math.min(Math.max(fieldColumn.implicitHeight + topPadding + bottomPadding, 80 * Appearance.factor),
                                             360 * Appearance.factor)
            contentWidth: availableWidth
            clip: true
            padding: 4 * Appearance.factor
            background: Rectangle {
                color: templateEditor.palette.base
                border.color: templateEditor.palette.mid
            }
            ExactColumn {
                id: fieldColumn
                width: fieldScroll.availableWidth
                Repeater {
                    model: fieldsModel
                    delegate: ExactRow {
                        id: fieldRow
                        required property int index
                        required property var model
                        Layout.fillWidth: true
                        gap: 4 * Appearance.factor
                        TextField {
                            visible: fieldRow.model.role === ""
                            text: fieldRow.model.label
                            placeholderText: qsTr("Name of the field")
                            Layout.fillWidth: true
                            onTextEdited: fieldsModel.setProperty(fieldRow.index, "label", text)
                        }
                        Label {
                            visible: fieldRow.model.role !== ""
                            text: fieldRow.model.name
                            Layout.fillWidth: true
                        }
                        ComboBox {
                            model: templateEditor.kinds
                            textRole: "name"
                            enabled: fieldRow.model.role === ""
                            currentIndex: templateEditor.kindIndex(fieldRow.model.kind)
                            onActivated: function(chosen) {
                                fieldsModel.setProperty(fieldRow.index, "kind", templateEditor.kinds[chosen].kind)
                            }
                        }
                        PictureButton {
                            text: "↑"
                            enabled: fieldRow.index > 0
                            onClicked: templateEditor.moveField(fieldRow.index, -1)
                        }
                        PictureButton {
                            text: "↓"
                            enabled: fieldRow.index < fieldsModel.count - 1
                            onClicked: templateEditor.moveField(fieldRow.index, 1)
                        }
                        PictureButton {
                            text: "×"
                            onClicked: fieldsModel.remove(fieldRow.index)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Remove field")
                        }
                    }
                }
            }
        }
        Button {
            objectName: "templateAddFieldButton"
            text: qsTr("Add a field")
            flat: true
            palette.buttonText: templateEditor.palette.link
            onClicked: fieldsModel.append({ role: "", label: qsTr("New field"), name: qsTr("New field"), kind: "text" })
        }
        }
    }
}
