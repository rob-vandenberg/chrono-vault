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
// ItemEditor.qml 0.3.62

// --- Version history ----------------------------------------------------
// v0.3.62: Save closes the editor on the saved item (the owner)
// v0.2.59: The labels and fields 50 to the left, as the view (the owner)
// v0.2.51: Its file dialogs as Chrono Viewer's (D-055)
// v0.1.41: The item's own Notes box; the reference program's field types and names (D-051)
// v0.0.40: Add at the left, Save and Cancel at the right; a click on the icon opens the icon picker (D-050)
// v0.0.18: As the reference program's editor (D-034): Cancel, "Add ▾" and a blue Save at the top; the icon with a
//          pencil badge; the title across the width; one row per field with its label at the right, the drag
//          handle and a blue "…" under the mouse; Field Options; sections as headings
// v0.0.17: Every part of an item and its fields kept through an edit (D-033)
// v0.0.14: Every template; fields moved up and down and removed; a field of several lines; tags; attachments
// v0.0.9: The delete question reachable from the Item menu (D-025)
// v0.0.7: Text sizes follow the UI scaling (D-023)
// v0.0.5: Whoever waits for the editor hears when the person cancels
// v0.0.4: One font in every field; the eye inside the password fields; the item's icon
// v0.0.3: Initial release

// One item being changed (D-034): its icon and title, its fields one per
// row, its tags and attachments. Nothing is kept until Save.
Item {
    id: editor

    property string itemId: ""
    property bool isNote: false
    property string templateId: ""
    property string categoryId: ""
    property string defaultIcon: ""
    property bool favourite: false
    // What this editor does not change, kept as it was (D-033).
    property var kept: ({})
    // Files being read for this item; their answers come from the controller.
    property var waitingFor: []
    property bool isNew: false
    property bool changed: false
    property string icon: ""   // the item's own icon, PNG in base64; empty for the category's

    property var afterAsking: null
    property var afterCancelling: null

    readonly property int labelWidth: Math.round(170 * Appearance.factor) // 50 to the left, as the view (the owner, 23 November 2026)
    readonly property color lineColour: Qt.rgba(palette.text.r, palette.text.g, palette.text.b, 0.14)
    // The reference program's field types, in the order its Field Options lists them.
    // The field types, with the reference program's names, in its order
    // (D-051).
    readonly property var kinds: [
        { kind: "text", name: qsTr("Text") },
        { kind: "password", name: qsTr("Password") },
        { kind: "pin", name: qsTr("PIN") },
        { kind: "numeric", name: qsTr("Numeric") },
        { kind: "date", name: qsTr("Date") },
        { kind: "email", name: qsTr("Email") },
        { kind: "url", name: qsTr("URL") },
        { kind: "phone", name: qsTr("Phone") },
        { kind: "username", name: qsTr("Username") },
        { kind: "totp", name: qsTr("One-time Code") },
        { kind: "multiline", name: qsTr("Multiline") },
        { kind: "ccName", name: qsTr("Cardholder Name") },
        { kind: "ccNumber", name: qsTr("Card Number") },
        { kind: "ccCvc", name: qsTr("CVC") },
        { kind: "ccPin", name: qsTr("Card PIN") },
        { kind: "ccExpiry", name: qsTr("Expiry Date") },
        { kind: "ccBankname", name: qsTr("Bank Name") },
        { kind: "ccTxnpassword", name: qsTr("Transaction Password") },
        { kind: "ccType", name: qsTr("Card Type") },
        { kind: "ccValidfrom", name: qsTr("Valid From") }
    ]

    // The kinds that hide their value from the start (D-051).
    function secretKind(kind) {
        return kind === "password" || kind === "ccPin" || kind === "pin" || kind === "ccTxnpassword"
    }

    signal closed()
    signal stored(string itemId)

    // Runs the action now, or, when there are changes not saved yet, after
    // asking whether to save them (OR-15). A file still being read holds it
    // up (D-029). When the person cancels, "abandoned" is run, if given.
    function whenFree(action, abandoned) {
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

    function edit(map, isNewItem) {
        itemId = map.id
        isNote = map.isNote
        templateId = map.templateId
        categoryId = map.categoryId
        defaultIcon = map.defaultIcon
        favourite = map.favourite
        isNew = isNewItem
        titleField.text = map.title
        icon = map.icon
        waitingFor = []
        fieldsModel.clear()
        for (let field of map.fields)
            fieldsModel.append({ role: field.role, label: field.label, name: field.name,
                                 kind: field.kind, value: field.value, sensitive: field.sensitive === true,
                                 deleted: field.deleted === true, hiddenType: field.hiddenType || "",
                                 uid: field.uid || 0 })
        kept = { subtitle: map.subtitle || "", autoSubmit: map.autoSubmit === true, website: map.website || "" }
        noteArea.text = map.note || ""
        tagsModel.clear()
        for (let tag of map.tags)
            tagsModel.append({ tag: tag })
        tagField.clear()
        attachmentsModel.clear()
        for (let attachment of map.attachments)
            attachmentsModel.append({ attachmentId: attachment.id, name: attachment.name, size: attachment.size })
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
                                       note: noteArea.text, autoSubmit: kept.autoSubmit, website: kept.website })) {
            isNew = false
            changed = false
            titleField.text = VaultController.item(itemId).title
            stored(itemId)
        }
    }

    function addField(kind) {
        const isSection = kind === "section"
        fieldsModel.append({ role: "", label: isSection ? qsTr("New section") : qsTr("New field"),
                             name: isSection ? qsTr("New section") : qsTr("New field"), kind: kind, value: "",
                             sensitive: editor.secretKind(kind), deleted: false, hiddenType: "",
                             uid: 0 })
        changed = true
        return fieldsModel.count - 1
    }

    // Moves a field by this many places (D-034: the drag handle).
    function moveField(index, step) {
        const target = index + step
        if (step === 0 || target < 0 || target >= fieldsModel.count)
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

    function kindName(kind) {
        for (const one of kinds) {
            if (one.kind === kind)
                return one.name
        }
        return kind === "section" ? qsTr("Section") : kind
    }

    Connections {
        target: VaultController
        function onAttachmentRead(requestId, name, size, ok) {
            if (editor.waitingFor.indexOf(requestId) < 0)
                return
            editor.waitingFor = editor.waitingFor.filter(function(one) { return one !== requestId })
            if (ok) {
                attachmentsModel.append({ attachmentId: requestId, name: name, size: size })
                editor.changed = true
            }
        }
    }

    ListModel { id: fieldsModel }
    ListModel { id: tagsModel }
    ListModel { id: attachmentsModel }

    ColumnLayout {
        anchors.fill: parent
        visible: editor.itemId !== ""
        spacing: 0

        // "Add ▾" at the left; the blue Save and Cancel at the right, 12
        // pixels between the buttons and the icon below (the owner, D-050).
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Math.round(16 * Appearance.factor)
            Layout.bottomMargin: Math.round(14 * Appearance.factor)
            Button {
                objectName: "addButton"
                text: qsTr("Add") + "  ▾"
                onClicked: addMenu.popup(this, 0, height)
                Menu {
                    id: addMenu
                    Action {
                        text: qsTr("Attachment")
                        onTriggered: attachmentDialog.open()
                    }
                    Action {
                        text: qsTr("Section")
                        onTriggered: fieldOptions.showForNew("section")
                    }
                    Action {
                        text: qsTr("Field")
                        onTriggered: fieldOptions.showForNew("text")
                    }
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                objectName: "saveButton"
                text: qsTr("Save")
                highlighted: true
                // Not while a file is still being read: it would be left out.
                enabled: (editor.changed || editor.isNew) && editor.waitingFor.length === 0
                // Stored, then the editor closes on the saved item (the owner, 0.3.62).
                onClicked: {
                    editor.store()
                    if (!editor.changed && !editor.isNew)
                        editor.close()
                }
            }
            Button {
                objectName: "cancelButton"
                text: qsTr("Cancel")
                // Cancel means: leave without keeping the changes.
                // With changes not saved yet, asked first, never lost by one click
                // (OR-15); without, the editor just closes.
                onClicked: editor.whenFree(editor.close)
            }
        }

        // The icon with its pencil badge, and the title across the width.
        RowLayout {
            spacing: Math.round(16 * Appearance.factor)
            Layout.fillWidth: true
            Layout.leftMargin: Math.round(16 * Appearance.factor)
            Layout.rightMargin: Math.round(16 * Appearance.factor)
            Layout.bottomMargin: Math.round(16 * Appearance.factor)
            // A click anywhere on the icon opens the icon picker (D-050).
            Item {
                objectName: "itemIcon"
                Layout.preferredWidth: Math.round(52 * Appearance.factor)
                Layout.preferredHeight: Math.round(52 * Appearance.factor)
                Image {
                    objectName: "itemIconImage"
                    anchors.fill: parent
                    source: editor.icon !== "" ? "data:image/png;base64," + editor.icon : editor.defaultIcon
                    sourceSize: Qt.size(width, height)
                }
                ToolButton {
                    objectName: "iconBadge"
                    width: Math.round(22 * Appearance.factor)
                    height: width
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: -Math.round(4 * Appearance.factor)
                    icon.source: "qrc:/icons/edit.svg"
                    icon.color: editor.palette.buttonText
                    icon.width: Math.round(12 * Appearance.factor)
                    icon.height: Math.round(12 * Appearance.factor)
                    background: Rectangle {
                        radius: width / 2
                        color: editor.palette.window
                        border.color: editor.lineColour
                    }
                    onClicked: editor.chooseIcon()
                }
                MouseArea {
                    objectName: "itemIconArea"
                    anchors.fill: parent
                    z: -1 // the badge, on top, answers for itself
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: editor.chooseIcon()
                    ToolTip.visible: containsMouse
                    ToolTip.text: qsTr("Change the icon")
                }
            }
            TextField {
                id: titleField
                objectName: "titleField"
                placeholderText: editor.isNote ? qsTr("Title of the note") : qsTr("Title, for example the site's name")
                Layout.fillWidth: true
                onTextEdited: editor.changed = true
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: editor.palette.mid
        }

        ScrollView {
            id: scroller
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: scroller.availableWidth - 2 * Math.round(24 * Appearance.factor)
                x: Math.round(24 * Appearance.factor)
                spacing: Math.round(4 * Appearance.factor)

                Item { Layout.preferredHeight: Math.round(12 * Appearance.factor) }

                Column {
                    id: fieldColumn
                    Layout.fillWidth: true
                    spacing: Math.round(4 * Appearance.factor)

                    // The row whose top is nearest to y, for dragging.
                    function indexAt(y) {
                        let found = -1
                        for (let index = 0; index < fieldRows.count; ++index) {
                            const row = fieldRows.itemAt(index)
                            if (row && row.visible && y >= row.y)
                                found = index
                        }
                        return found
                    }

                    Repeater {
                        id: fieldRows
                        model: fieldsModel
                        delegate: Item {
                            id: fieldRow
                            required property int index
                            required property var model
                            readonly property bool isSection: model.kind === "section"
                            readonly property bool isSecret: model.kind === "password" || model.sensitive === true
                            // Shown under the mouse; always on a touch screen, which has no hover.
                            readonly property bool active: rowHover.hovered || dragArea.pressed || VaultController.touchScreen
                            // Kept but never shown, as in the reference program (D-033).
                            visible: !(model.deleted === true || model.kind === "hidden")
                            width: fieldColumn.width
                            height: visible ? Math.max(Math.round(44 * Appearance.factor),
                                                       line.implicitHeight + Math.round(12 * Appearance.factor)) : 0

                            HoverHandler { id: rowHover }
                            Rectangle {
                                anchors.fill: parent
                                radius: Math.round(4 * Appearance.factor)
                                color: Qt.rgba(editor.palette.text.r, editor.palette.text.g, editor.palette.text.b, 0.06)
                                visible: fieldRow.active
                            }

                            RowLayout {
                                id: line
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: Math.round(12 * Appearance.factor)

                                // The drag handle: drag it up or down to move the field.
                                Label {
                                    objectName: "dragHandle"
                                    text: "≡"
                                    font.pointSize: Appearance.fontPointSize * 1.3
                                    opacity: fieldRow.active ? 0.8 : 0
                                    Layout.leftMargin: Math.round(10 * Appearance.factor)
                                    MouseArea {
                                        id: dragArea
                                        anchors.fill: parent
                                        anchors.margins: -Math.round(6 * Appearance.factor)
                                        cursorShape: Qt.SizeVerCursor
                                        onPositionChanged: function(mouse) {
                                            const point = mapToItem(fieldColumn, mouse.x, mouse.y)
                                            const target = fieldColumn.indexAt(point.y)
                                            if (target >= 0 && target !== fieldRow.index)
                                                editor.moveField(fieldRow.index, target - fieldRow.index)
                                        }
                                    }
                                }
                                Label {
                                    visible: !fieldRow.isSection
                                    text: fieldRow.model.role === "" ? fieldRow.model.label : fieldRow.model.name
                                    textFormat: Text.PlainText
                                    horizontalAlignment: Text.AlignRight
                                    opacity: 0.7
                                    elide: Text.ElideRight
                                    Layout.preferredWidth: editor.labelWidth - Math.round(30 * Appearance.factor)
                                    Layout.alignment: fieldRow.model.kind === "multiline" ? Qt.AlignTop : Qt.AlignVCenter
                                    Layout.topMargin: fieldRow.model.kind === "multiline" ? Math.round(8 * Appearance.factor) : 0
                                }
                                // A section's heading, bold, at the value column.
                                Label {
                                    objectName: "editorSectionHeading"
                                    visible: fieldRow.isSection
                                    text: fieldRow.model.label
                                    textFormat: Text.PlainText
                                    font.bold: true
                                    opacity: 0.7
                                    leftPadding: editor.labelWidth - Math.round(18 * Appearance.factor)
                                    Layout.fillWidth: true
                                    Layout.topMargin: Math.round(10 * Appearance.factor)
                                }
                                TextField {
                                    visible: !fieldRow.isSection && !fieldRow.isSecret && fieldRow.model.kind !== "multiline"
                                    text: fieldRow.model.value
                                    font.family: fieldRow.model.kind === "totp" ? "Noto Sans Mono" : Appearance.font.family
                                    placeholderText: fieldRow.model.kind === "totp" ? qsTr("Enter secret key") : ""
                                    Layout.fillWidth: true
                                    onTextEdited: {
                                        fieldsModel.setProperty(fieldRow.index, "value", text)
                                        editor.changed = true
                                    }
                                }
                                PasswordField {
                                    visible: !fieldRow.isSection && fieldRow.isSecret
                                    text: fieldRow.model.value
                                    Layout.fillWidth: true
                                    onTextEdited: {
                                        fieldsModel.setProperty(fieldRow.index, "value", text)
                                        editor.changed = true
                                    }
                                }
                                TextArea {
                                    visible: !fieldRow.isSection && !fieldRow.isSecret && fieldRow.model.kind === "multiline"
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
                                Button {
                                    visible: fieldRow.model.kind === "password"
                                    text: qsTr("Generate")
                                    onClicked: generator.openFor(fieldRow.index)
                                }
                                // Field Options, in blue, under the mouse.
                                ToolButton {
                                    objectName: "fieldOptionsButton"
                                    text: "…"
                                    palette.buttonText: editor.palette.highlight
                                    opacity: fieldRow.active ? 1 : 0
                                    Layout.alignment: fieldRow.model.kind === "multiline" ? Qt.AlignTop : Qt.AlignVCenter
                                    onClicked: fieldOptions.showFor(fieldRow.index, false)
                                }
                            }
                        }
                    }
                }

                // The item's own Notes: every item has exactly one, always
                // shown here, never deleted (the owner, D-051).
                Label {
                    text: qsTr("Notes")
                    opacity: 0.7
                    Layout.topMargin: Math.round(16 * Appearance.factor)
                }
                TextArea {
                    id: noteArea
                    objectName: "noteArea"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(implicitHeight, Math.round(120 * Appearance.factor))
                    placeholderText: qsTr("Add Note")
                    wrapMode: TextEdit.Wrap
                    onTextChanged: if (activeFocus) editor.changed = true
                }

                // Tags (D-028).
                Label {
                    text: qsTr("Tags")
                    opacity: 0.7
                    Layout.topMargin: Math.round(16 * Appearance.factor)
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
                    visible: attachmentsModel.count > 0 || editor.waitingFor.length > 0
                    text: qsTr("Attachments")
                    opacity: 0.7
                    Layout.topMargin: Math.round(16 * Appearance.factor)
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
                Label {
                    visible: editor.waitingFor.length > 0
                    text: qsTr("Reading the file…")
                    opacity: 0.7
                }

                Item { Layout.preferredHeight: Math.round(24 * Appearance.factor) }
            }
        }
    }

    // A field's options (D-034): its name, its type, whether it is hidden,
    // and Delete Field; a section has only its name.
    DialogWindow {
        id: fieldOptions
        objectName: "fieldOptions"
        memoryName: "fieldOptions"
        property int fieldIndex: -1
        property bool isSection: false
        property bool isNewField: false
        property bool changedBefore: false
        title: qsTr("Field Options")
        // A field added through "Add ▾": closed without Save, the editor is
        // as it was, unsaved changes and all.
        function showForNew(kind) {
            changedBefore = editor.changed
            showFor(editor.addField(kind), true)
        }
        standardButtons: Dialog.Save
        function showFor(index, isNewField) {
            const field = fieldsModel.get(index)
            fieldIndex = index
            fieldOptions.isNewField = isNewField
            isSection = field.kind === "section"
            nameField.text = field.role === "" ? field.label : field.name
            nameField.enabled = field.role === ""
            let chosen = 0
            for (let at = 0; at < editor.kinds.length; ++at) {
                if (editor.kinds[at].kind === field.kind)
                    chosen = at
            }
            typeBox.currentIndex = chosen
            sensitiveSwitch.checked = field.sensitive === true || field.kind === "password"
            open()
            nameField.forceActiveFocus()
            nameField.selectAll()
        }
        leftButtons: Button {
            objectName: "deleteFieldButton"
            text: qsTr("Delete Field")
            palette.buttonText: "#e53e3e"
            onClicked: {
                fieldsModel.remove(fieldOptions.fieldIndex)
                editor.changed = true
                fieldOptions.isNewField = false
                // The field is gone: nothing of this window goes to the next one.
                fieldOptions.fieldIndex = -1
                fieldOptions.accept()
            }
        }
        onAccepted: {
            if (fieldIndex < 0 || fieldIndex >= fieldsModel.count)
                return
            if (fieldsModel.get(fieldIndex).role === "") {
                const name = nameField.text.trim() === "" ? (isSection ? qsTr("Section") : qsTr("Field"))
                                                           : nameField.text.trim()
                fieldsModel.setProperty(fieldIndex, "label", name)
                fieldsModel.setProperty(fieldIndex, "name", name)
            }
            if (!isSection) {
                const kind = editor.kinds[typeBox.currentIndex].kind
                fieldsModel.setProperty(fieldIndex, "kind", kind)
                fieldsModel.setProperty(fieldIndex, "sensitive", sensitiveSwitch.checked || kind === "password")
            }
            editor.changed = true
        }
        // Closed without Save: a field just added is not added after all.
        onRejected: {
            if (isNewField && fieldIndex >= 0 && fieldIndex < fieldsModel.count) {
                fieldsModel.remove(fieldIndex)
                editor.changed = changedBefore
            }
            isNewField = false
        }
        GridLayout {
            columns: 2
            columnSpacing: Math.round(24 * Appearance.factor)
            rowSpacing: Math.round(12 * Appearance.factor)
            Layout.preferredWidth: Math.round(420 * Appearance.factor)
            Label { text: fieldOptions.isSection ? qsTr("Section Name") : qsTr("Field Name") }
            TextField {
                id: nameField
                objectName: "fieldNameField"
                Layout.fillWidth: true
                onAccepted: fieldOptions.accept()
            }
            Label {
                visible: !fieldOptions.isSection
                text: qsTr("Field Type")
            }
            ComboBox {
                id: typeBox
                objectName: "fieldTypeBox"
                visible: !fieldOptions.isSection
                model: editor.kinds
                textRole: "name"
                Layout.fillWidth: true
                // A kind that hides its value starts hidden (D-051).
                onActivated: function(index) {
                    if (editor.secretKind(editor.kinds[index].kind))
                        sensitiveSwitch.checked = true
                }
            }
            Label {
                visible: !fieldOptions.isSection
                text: qsTr("Sensitive")
            }
            Switch {
                id: sensitiveSwitch
                objectName: "sensitiveSwitch"
                visible: !fieldOptions.isSection
                Layout.alignment: Qt.AlignRight
            }
        }
    }

    // The icon picker (D-050): the chosen icon, or the category's.
    function chooseIcon() {
        iconPicker.start(editor.defaultIcon)
    }
    IconPicker {
        id: iconPicker
        onIconChosen: function(chosen) {
            if (chosen !== editor.icon) {
                editor.icon = chosen
                editor.changed = true
            }
        }
        onIconDeleted: function(deleted) {
            if (deleted === editor.icon) {
                editor.icon = ""
                editor.changed = true
            }
        }
    }

    FilePicker {
        id: attachmentDialog
        memoryName: "ItemEditor.attachmentDialog"
        parentWindow: editor
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
}
