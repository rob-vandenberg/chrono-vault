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
// ItemEditor.qml 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: One font in every field; the eye inside the password fields; the item's icon
// v0.0.3: Initial release

// One item: its title and fields, to read, copy and change.
Item {
    id: editor

    property string itemId: ""
    property bool isNote: false
    property bool isNew: false
    property bool changed: false
    property string icon: ""   // the item's own icon, PNG in base64; empty for the template's
    property int tick: 0

    property var afterAsking: null

    signal closed()
    signal stored(string itemId)

    // Runs the action now, or, when there are changes not saved yet, after
    // asking whether to save them (OR-15).
    function whenFree(action) {
        if (itemId !== "" && changed) {
            afterAsking = action
            unsavedDialog.open()
        } else {
            action()
        }
    }

    function edit(map, isNewItem) {
        itemId = map.id
        isNote = map.isNote
        isNew = isNewItem
        titleField.text = map.title
        icon = map.icon
        fieldsModel.clear()
        for (let field of map.fields)
            fieldsModel.append({ role: field.role, label: field.label, name: field.name,
                                 kind: field.kind, value: field.value, revealed: false })
        changed = false
        if (isNewItem)
            titleField.forceActiveFocus()
    }

    function close() {
        itemId = ""
        fieldsModel.clear()
        titleField.clear()
        changed = false
        closed()
    }

    function store() {
        let fields = []
        for (let index = 0; index < fieldsModel.count; ++index) {
            const field = fieldsModel.get(index)
            fields.push({ role: field.role, label: field.label, kind: field.kind, value: field.value })
        }
        if (VaultController.saveItem({ id: itemId, isNote: isNote, title: titleField.text, fields: fields,
                                       icon: icon })) {
            isNew = false
            changed = false
            titleField.text = VaultController.item(itemId).title
            stored(itemId)
        }
    }

    function addField(kind) {
        fieldsModel.append({ role: "", label: qsTr("New field"), name: qsTr("New field"),
                             kind: kind, value: "", revealed: false })
        changed = true
    }

    Timer {
        interval: 1000
        repeat: true
        running: editor.visible && editor.itemId !== ""
        onTriggered: editor.tick++
    }

    ListModel { id: fieldsModel }

    Label {
        anchors.centerIn: parent
        visible: editor.itemId === ""
        text: qsTr("Choose an item on the left, or make a new one.")
        opacity: 0.7
    }

    ScrollView {
        id: scroller
        anchors.fill: parent
        visible: editor.itemId !== ""
        contentWidth: availableWidth

        ColumnLayout {
            width: scroller.availableWidth - 24
            x: 12
            spacing: 10

            Item { Layout.preferredHeight: 4 }

            RowLayout {
                spacing: 12
                Layout.fillWidth: true

                Image {
                    objectName: "itemIconImage"
                    source: editor.icon !== "" ? "data:image/png;base64," + editor.icon
                            : (editor.isNote ? "qrc:/icons/item-note.svg" : "qrc:/icons/item-login.svg")
                    sourceSize: Qt.size(64, 64)
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 64
                }
                ColumnLayout {
                    spacing: 4
                    Button {
                        text: qsTr("Choose icon…")
                        onClicked: iconDialog.open()
                    }
                    Button {
                        text: qsTr("Remove icon")
                        enabled: editor.icon !== ""
                        onClicked: {
                            editor.icon = ""
                            editor.changed = true
                        }
                    }
                }
                Item { Layout.fillWidth: true }
            }

            TextField {
                id: titleField
                placeholderText: editor.isNote ? qsTr("Title of the note") : qsTr("Title, for example the site's name")
                font.pointSize: 14
                Layout.fillWidth: true
                onTextEdited: editor.changed = true
            }

            Repeater {
                model: fieldsModel

                delegate: ColumnLayout {
                    id: fieldRow
                    required property int index
                    required property var model
                    readonly property bool isPassword: model.kind === "password"
                    readonly property bool isCode: model.kind === "totp"
                    readonly property bool isCustom: model.role === ""
                    spacing: 2
                    Layout.fillWidth: true

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            visible: !fieldRow.isCustom
                            text: fieldRow.model.name
                            opacity: 0.8
                        }
                        TextField {
                            visible: fieldRow.isCustom
                            text: fieldRow.model.label
                            placeholderText: qsTr("Name of the field")
                            Layout.preferredWidth: 220
                            onTextEdited: {
                                fieldsModel.setProperty(fieldRow.index, "label", text)
                                editor.changed = true
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            visible: fieldRow.isCustom
                            flat: true
                            text: qsTr("Remove field")
                            onClicked: {
                                fieldsModel.remove(fieldRow.index)
                                editor.changed = true
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: fieldRow.model.kind !== "multiline"

                        TextField {
                            visible: !fieldRow.isPassword
                            text: fieldRow.model.value
                            placeholderText: fieldRow.isCode ? qsTr("The secret the site gives, or its otpauth:// address") : ""
                            Layout.fillWidth: true
                            onTextEdited: {
                                fieldsModel.setProperty(fieldRow.index, "value", text)
                                editor.changed = true
                            }
                        }
                        PasswordField {
                            visible: fieldRow.isPassword
                            text: fieldRow.model.value
                            Layout.fillWidth: true
                            onTextEdited: {
                                fieldsModel.setProperty(fieldRow.index, "value", text)
                                editor.changed = true
                            }
                        }
                        Button {
                            visible: fieldRow.isPassword
                            text: qsTr("Generate…")
                            onClicked: generator.openFor(fieldRow.index)
                        }
                        Button {
                            visible: !fieldRow.isCode
                            enabled: fieldRow.model.value !== ""
                            text: qsTr("Copy")
                            onClicked: VaultController.copyToClipboard(fieldRow.model.value)
                        }
                    }

                    RowLayout {
                        visible: fieldRow.isCode && fieldRow.model.value !== ""
                        readonly property string code: { editor.tick; return VaultController.oneTimeCode(fieldRow.model.value) }
                        Label {
                            text: parent.code === "" ? qsTr("This is not a valid one-time code secret.") : parent.code
                            font.family: "Noto Sans Mono"
                            font.pointSize: parent.code === "" ? 10 : 16
                        }
                        Label {
                            visible: parent.code !== ""
                            text: { editor.tick; return qsTr("%n second(s) left", "", VaultController.oneTimeCodeSecondsLeft(fieldRow.model.value)) }
                            opacity: 0.7
                        }
                        Button {
                            visible: parent.code !== ""
                            text: qsTr("Copy code")
                            onClicked: VaultController.copyToClipboard(VaultController.oneTimeCode(fieldRow.model.value))
                        }
                    }

                    TextArea {
                        visible: fieldRow.model.kind === "multiline"
                        text: fieldRow.model.value
                        wrapMode: TextEdit.Wrap
                        Layout.fillWidth: true
                        Layout.minimumHeight: 80
                        onTextChanged: {
                            if (text !== fieldRow.model.value) {
                                fieldsModel.setProperty(fieldRow.index, "value", text)
                                editor.changed = true
                            }
                        }
                    }
                }
            }

            RowLayout {
                Button {
                    text: qsTr("Add a text field")
                    flat: true
                    onClicked: editor.addField("text")
                }
                Button {
                    text: qsTr("Add a password field")
                    flat: true
                    onClicked: editor.addField("password")
                }
            }

            RowLayout {
                Layout.topMargin: 8
                Button {
                    text: qsTr("Save")
                    highlighted: true
                    enabled: editor.changed || editor.isNew
                    onClicked: editor.store()
                }
                Button {
                    text: editor.isNew ? qsTr("Cancel") : qsTr("Close")
                    onClicked: editor.whenFree(editor.close)
                }
                Item { Layout.fillWidth: true }
                Button {
                    visible: !editor.isNew
                    text: qsTr("Delete…")
                    onClicked: deleteDialog.open()
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }

    FileDialog {
        id: iconDialog
        title: qsTr("Choose an icon")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Pictures (*.jpg *.jpeg *.png *.bmp *.ico *.svg)")]
        onAccepted: {
            const picked = VaultController.readItemIcon(selectedFile)
            if (picked !== "") {
                editor.icon = picked
                editor.changed = true
            }
        }
    }

    GeneratorDialog {
        id: generator
        property int fieldIndex: -1
        function openFor(index) {
            fieldIndex = index
            open()
        }
        onUsePassword: function(password) {
            fieldsModel.setProperty(fieldIndex, "value", password)
            editor.changed = true
        }
    }

    DialogWindow {

        id: unsavedDialog

        memoryName: "unsavedChanges"
        objectName: "unsavedDialog"
        title: qsTr("Save your changes?")
        standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel
        ColumnLayout {
            Label {
                text: qsTr("“%1” has changes that are not saved yet.").arg(titleField.text)
                wrapMode: Text.Wrap
                Layout.preferredWidth: 360
            }
        }
        onAccepted: {
            const action = editor.afterAsking
            editor.afterAsking = null
            editor.store()
            if (!editor.changed && action)
                action()
        }
        onDiscarded: {
            const action = editor.afterAsking
            editor.afterAsking = null
            editor.changed = false
            unsavedDialog.close()
            if (action)
                action()
        }
        onRejected: editor.afterAsking = null
    }

    DialogWindow {

        id: deleteDialog

        memoryName: "deleteItem"
        title: qsTr("Delete this item?")
        standardButtons: Dialog.Yes | Dialog.No
        ColumnLayout {
            Label {
                text: qsTr("“%1” is removed from the vault. This cannot be undone.").arg(titleField.text)
                wrapMode: Text.Wrap
                Layout.preferredWidth: 360
            }
        }
        onAccepted: {
            VaultController.deleteItem(editor.itemId)
            editor.close()
        }
    }
}
