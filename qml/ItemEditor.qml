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
// ItemEditor.qml 0.0.17

// --- Version history ----------------------------------------------------
// v0.0.17: Every part of an item and its fields kept through an edit: hide marks, ids, hidden and deleted fields,
//          subtitle, note, autosubmit (D-033)
// v0.0.14: Every template; fields moved up and down and removed; a field of several lines; tags; attachments
//          (D-027, D-028, D-029)
// v0.0.9: The delete question reachable from the Item menu (D-025)
// v0.0.7: Text sizes follow the UI scaling (D-023)
// v0.0.5: Whoever waits for the editor hears when the person cancels
// v0.0.4: One font in every field; the eye inside the password fields; the item's icon
// v0.0.3: Initial release

// One item: its title and fields, to read, copy and change.
Item {
    id: editor

    property string itemId: ""
    property bool isNote: false
    property string templateId: ""
    property string categoryId: ""
    property string defaultIcon: ""
    property bool favourite: false
    // What this editor does not show yet, kept as it was (D-033).
    property var kept: ({})
    // Files being read for this item; their answers come from the controller.
    property var waitingFor: []
    property bool isNew: false
    property bool changed: false
    property string icon: ""   // the item's own icon, PNG in base64; empty for the template's
    property int tick: 0

    property var afterAsking: null
    property var afterCancelling: null

    signal closed()
    signal stored(string itemId)

    // Runs the action now, or, when there are changes not saved yet, after
    // asking whether to save them (OR-15). When the person cancels, or the
    // save does not succeed, the action is not run and "abandoned" is, if given.
    function whenFree(action, abandoned) {
        // A file still being read would be lost by leaving now (D-029).
        if (itemId !== "" && waitingFor.length > 0) {
            readingDialog.open()
            if (abandoned)
                abandoned()
            return
        }
        if (itemId !== "" && changed) {
            afterAsking = action
            afterCancelling = abandoned === undefined ? null : abandoned
            unsavedDialog.open()
        } else {
            action()
        }
    }

    // Item > Delete item… (D-025): the same question as the Delete button.
    function askDelete() {
        if (itemId !== "" && !isNew)
            deleteDialog.open()
    }

    function edit(map, isNewItem) {
        itemId = map.id
        isNote = map.isNote
        templateId = map.templateId
        categoryId = map.categoryId
        defaultIcon = map.defaultIcon
        favourite = map.favourite
        waitingFor = []
        tagsModel.clear()
        for (let tag of map.tags)
            tagsModel.append({ tag: tag })
        tagField.clear()
        attachmentsModel.clear()
        for (let attachment of map.attachments)
            attachmentsModel.append({ attachmentId: attachment.id, name: attachment.name, size: attachment.size })
        isNew = isNewItem
        titleField.text = map.title
        icon = map.icon
        fieldsModel.clear()
        for (let field of map.fields)
            fieldsModel.append({ role: field.role, label: field.label, name: field.name,
                                 kind: field.kind, value: field.value, revealed: false,
                                 sensitive: field.sensitive === true, deleted: field.deleted === true,
                                 hiddenType: field.hiddenType || "", uid: field.uid || 0 })
        kept = { subtitle: map.subtitle || "", note: map.note || "", autoSubmit: map.autoSubmit === true,
                 website: map.website || "" }
        changed = false
        if (isNewItem)
            titleField.forceActiveFocus()
    }

    function close() {
        itemId = ""
        fieldsModel.clear()
        tagsModel.clear()
        attachmentsModel.clear()
        waitingFor = []
        tagField.clear()
        titleField.clear()
        changed = false
        closed()
    }

    function store() {
        let fields = []
        for (let index = 0; index < fieldsModel.count; ++index) {
            const field = fieldsModel.get(index)
            fields.push({ role: field.role, label: field.label, kind: field.kind, value: field.value,
                          sensitive: field.sensitive, deleted: field.deleted, hiddenType: field.hiddenType,
                          uid: field.uid })
        }
        // A tag typed but not yet added with Enter counts too.
        addTag()
        let tags = []
        for (let index = 0; index < tagsModel.count; ++index)
            tags.push(tagsModel.get(index).tag)
        let attachments = []
        for (let index = 0; index < attachmentsModel.count; ++index)
            attachments.push({ id: attachmentsModel.get(index).attachmentId })
        if (VaultController.saveItem({ id: itemId, templateId: templateId, categoryId: categoryId,
                                       title: titleField.text, fields: fields, icon: icon, tags: tags,
                                       favourite: favourite, attachments: attachments, subtitle: kept.subtitle,
                                       note: kept.note, autoSubmit: kept.autoSubmit, website: kept.website })) {
            isNew = false
            changed = false
            titleField.text = VaultController.item(itemId).title
            stored(itemId)
        }
    }

    function addField(kind) {
        fieldsModel.append({ role: "", label: qsTr("New field"), name: qsTr("New field"),
                             kind: kind, value: "", revealed: false, sensitive: kind === "password",
                             deleted: false, hiddenType: "", uid: 0 })
        changed = true
    }

    // Moves a field one place up (-1) or down (1).
    function moveField(index, step) {
        const target = index + step
        if (target < 0 || target >= fieldsModel.count)
            return
        fieldsModel.move(index, target, 1)
        changed = true
    }

    // The tag typed in the tag field, kept once (D-028).
    function addTag() {
        const tag = tagField.text.split("/").map(function(part) { return part.trim() })
                            .filter(function(part) { return part !== "" }).join("/")
        tagField.clear()
        if (tag === "")
            return
        for (let index = 0; index < tagsModel.count; ++index) {
            if (tagsModel.get(index).tag === tag)
                return
        }
        tagsModel.append({ tag: tag })
        changed = true
    }

    function removeTag(index) {
        tagsModel.remove(index)
        changed = true
    }

    // A file the person picked: read off the window's thread (D-029).
    function addAttachment(file) {
        const requestId = VaultController.readAttachment(file)
        if (requestId !== "")
            waitingFor = waitingFor.concat([requestId])
    }

    Connections {
        target: VaultController
        function onAttachmentRead(requestId, name, size, ok) {
            const at = editor.waitingFor.indexOf(requestId)
            if (at < 0)
                return
            editor.waitingFor = editor.waitingFor.filter(function(one) { return one !== requestId })
            if (ok) {
                attachmentsModel.append({ attachmentId: requestId, name: name, size: size })
                editor.changed = true
            }
        }
    }

    ListModel { id: tagsModel }
    ListModel { id: attachmentsModel }

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
            width: scroller.availableWidth - Math.round(24 * Appearance.factor)
            x: 12
            spacing: Math.round(10 * Appearance.factor)

            Item { Layout.preferredHeight: 4 }

            RowLayout {
                spacing: Math.round(12 * Appearance.factor)
                Layout.fillWidth: true

                Image {
                    objectName: "itemIconImage"
                    source: editor.icon !== "" ? "data:image/png;base64," + editor.icon : editor.defaultIcon
                    sourceSize: Qt.size(Math.round(64 * Appearance.factor), Math.round(64 * Appearance.factor))
                    Layout.preferredWidth: Math.round(64 * Appearance.factor)
                    Layout.preferredHeight: Math.round(64 * Appearance.factor)
                }
                ColumnLayout {
                    spacing: Math.round(4 * Appearance.factor)
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
                font.pointSize: Appearance.fontPointSize * 1.4
                Layout.fillWidth: true
                onTextEdited: editor.changed = true
            }

            Repeater {
                model: fieldsModel

                delegate: ColumnLayout {
                    id: fieldRow
                    required property int index
                    required property var model
                    readonly property bool isPassword: model.kind === "password" || model.sensitive === true
                    readonly property bool isCode: model.kind === "totp"
                    readonly property bool isCustom: model.role === ""
                    // Kept but never shown, as in the reference program (D-033).
                    visible: !(model.deleted === true || model.kind === "hidden")
                    spacing: Math.round(2 * Appearance.factor)
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
                            Layout.preferredWidth: Math.round(220 * Appearance.factor)
                            onTextEdited: {
                                fieldsModel.setProperty(fieldRow.index, "label", text)
                                editor.changed = true
                            }
                        }
                        Item { Layout.fillWidth: true }
                        ToolButton {
                            objectName: "fieldUpButton"
                            text: "↑"
                            enabled: fieldRow.index > 0
                            onClicked: editor.moveField(fieldRow.index, -1)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Move the field up")
                        }
                        ToolButton {
                            objectName: "fieldDownButton"
                            text: "↓"
                            enabled: fieldRow.index < fieldsModel.count - 1
                            onClicked: editor.moveField(fieldRow.index, 1)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Move the field down")
                        }
                        Button {
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
                            font.pointSize: Appearance.fontPointSize * (parent.code === "" ? 1.0 : 1.6)
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
                        Layout.minimumHeight: Math.round(80 * Appearance.factor)
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
                Button {
                    text: qsTr("Add a field of several lines")
                    flat: true
                    onClicked: editor.addField("multiline")
                }
            }

            // Tags (D-028): each shown with a cross to take it off; a new one
            // typed below, a slash nesting it under another.
            Label {
                text: qsTr("Tags")
                opacity: 0.8
                Layout.topMargin: Math.round(6 * Appearance.factor)
            }
            Flow {
                Layout.fillWidth: true
                spacing: Math.round(6 * Appearance.factor)
                visible: tagsModel.count > 0
                Repeater {
                    model: tagsModel
                    delegate: Button {
                        required property int index
                        required property string tag
                        objectName: "tagChip"
                        text: tag + "  ×"
                        flat: true
                        onClicked: editor.removeTag(index)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove this tag")
                    }
                }
            }
            TextField {
                id: tagField
                objectName: "tagField"
                placeholderText: qsTr("Add a tag, for example Work/Servers, and press Enter")
                Layout.fillWidth: true
                onAccepted: editor.addTag()
            }

            // Attachments (D-029): kept inside the vault, sealed.
            Label {
                text: qsTr("Attachments")
                opacity: 0.8
                Layout.topMargin: Math.round(6 * Appearance.factor)
            }
            Repeater {
                model: attachmentsModel
                delegate: RowLayout {
                    required property int index
                    required property string name
                    required property double size
                    Layout.fillWidth: true
                    Image {
                        source: "qrc:/icons/attachment.svg"
                        sourceSize: Qt.size(Math.round(16 * Appearance.factor), Math.round(16 * Appearance.factor))
                    }
                    Label {
                        text: name
                        textFormat: Text.PlainText
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                    Label {
                        text: VaultController.sizeText(size)
                        opacity: 0.7
                    }
                    Button {
                        flat: true
                        text: qsTr("Remove")
                        onClicked: {
                            attachmentsModel.remove(index)
                            editor.changed = true
                        }
                    }
                }
            }
            RowLayout {
                Button {
                    objectName: "addAttachmentButton"
                    text: qsTr("Add attachment…")
                    flat: true
                    onClicked: attachmentDialog.open()
                }
                Label {
                    visible: editor.waitingFor.length > 0
                    text: qsTr("Reading the file…")
                    opacity: 0.7
                }
            }

            RowLayout {
                Layout.topMargin: Math.round(8 * Appearance.factor)
                Button {
                    text: qsTr("Save")
                    highlighted: true
                    // Not while a file is still being read: it would be left out.
                    enabled: (editor.changed || editor.isNew) && editor.waitingFor.length === 0
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

    FileDialog {
        id: attachmentDialog
        title: qsTr("Add an attachment")
        fileMode: FileDialog.OpenFile
        onAccepted: editor.addAttachment(selectedFile)
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
                Layout.preferredWidth: Math.round(360 * Appearance.factor)
            }
        }
        onAccepted: {
            const action = editor.afterAsking
            const abandoned = editor.afterCancelling
            editor.afterAsking = null
            editor.afterCancelling = null
            editor.store()
            if (!editor.changed && action)
                action()
            else if (editor.changed && abandoned)
                abandoned()
        }
        onDiscarded: {
            const action = editor.afterAsking
            editor.afterAsking = null
            editor.afterCancelling = null
            editor.changed = false
            unsavedDialog.close()
            if (action)
                action()
        }
        onRejected: {
            const abandoned = editor.afterCancelling
            editor.afterAsking = null
            editor.afterCancelling = null
            if (abandoned)
                abandoned()
        }
    }

    DialogWindow {
        id: readingDialog
        objectName: "readingDialog"
        memoryName: "readingAttachment"
        title: qsTr("A file is being read")
        standardButtons: Dialog.Ok
        ColumnLayout {
            Label {
                text: qsTr("The file you added is still being read. Try again in a moment.")
                wrapMode: Text.Wrap
                Layout.preferredWidth: Math.round(360 * Appearance.factor)
            }
        }
    }

    DialogWindow {

        id: deleteDialog
        objectName: "deleteDialog"

        memoryName: "deleteItem"
        title: qsTr("Move this item to the Trash?")
        standardButtons: Dialog.Yes | Dialog.No
        ColumnLayout {
            Label {
                text: qsTr("“%1” is moved to the Trash, where it can be restored.").arg(titleField.text)
                wrapMode: Text.Wrap
                Layout.preferredWidth: Math.round(360 * Appearance.factor)
            }
        }
        onAccepted: {
            VaultController.moveToTrash(editor.itemId, true)
            editor.close()
        }
    }
}
