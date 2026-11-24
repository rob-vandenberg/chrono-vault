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
// ItemDetails.qml 0.5.82

// --- Version history ----------------------------------------------------
// v0.5.82: Trash, Restore and Archive through the page, asked first (D-062)
// v0.5.78: Texts in dialogs wrap with the window (D-061)
// v0.5.76: A sync conflict settled in the Sync conflicts dialog, no longer on the item (D-060)
// v0.2.51: Its file dialogs as Chrono Viewer's (D-055)
// v0.2.46: The labels end 50 pixels further left (the owner)
// v0.2.45: The notes a merge left, with Use the other value, Delete Permanently and OK or Keep (D-054)
// v0.1.41: The cards on the theme's Base colour (D-051)
// v0.0.24: Item > Show Passwords shows every sensitive field (D-039)
// v0.0.17: As the reference program shows an item (D-033): every section its own card under its heading, every
//          field type, hidden values as dots, History in a field's menu, the note, Last Modified and Created,
//          Autosubmit Login, the Trash and the Archive
// v0.0.14: The icon of the item's category; the star that marks a favourite; the template under the title; the
//          tags; the attachments, each saved with Save as… (D-027, D-028, D-029)
// v0.0.9: Initial release (D-025)

// The chosen item, read-only (D-025, D-033): its header with the pencil, the
// star and "…", then its fields in framed cards, one per section, the
// label at the right in grey, the value beside it, a "…" menu at the end of
// each row. Empty, deleted and hidden fields are not shown.
Item {
    id: itemDetails
    objectName: "itemDetails"

    property string itemId: ""
    // Item > Show Passwords: every sensitive field as text (D-039).
    property bool revealAll: false
    // Read again whenever the vault's items change.
    property int refresh: 0
    readonly property var item: itemId !== "" && refresh >= 0 ? VaultController.item(itemId) : ({})
    // What a merge left on the item to settle (D-054).
    readonly property var mergeNotes: itemId !== "" && refresh >= 0 ? VaultController.mergeNotes(itemId) : []

    // The fields as cards: [{heading, rows: [field…]}]; a section without a
    // field to show is left out, as the reference program does (D-033).
    readonly property var cards: {
        const all = item.fields || []
        let result = [{ heading: "", rows: [] }]
        for (const field of all) {
            if (field.deleted || field.kind === "hidden")
                continue
            if (field.kind === "section") {
                result.push({ heading: field.name, rows: [] })
                continue
            }
            if (field.value === "")
                continue
            result[result.length - 1].rows.push(field)
        }
        return result.filter(function(card) { return card.rows.length > 0 })
    }
    readonly property int labelWidth: Math.round(170 * Appearance.factor) // 50 to the left (the owner, 23 November 2026)
    readonly property int rowHeight: Math.round(54 * Appearance.factor)
    // Card borders and the lines between rows: a faint shade of the text,
    // seen on a light and a dark page alike.
    readonly property color lineColour: Qt.rgba(palette.text.r, palette.text.g, palette.text.b, 0.14)

    signal editRequested()
    signal trashRequested()
    signal restoreRequested()
    signal archiveRequested()
    signal conflictRequested(string itemId)

    Connections {
        target: VaultController.items
        function onModelReset() { itemDetails.refresh++ }
    }

    // One-time codes count down every second.
    property int tick: 0
    Timer {
        interval: 1000
        repeat: true
        running: itemDetails.visible && itemDetails.itemId !== ""
        onTriggered: itemDetails.tick++
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // The header: icon, title, and the pencil, the star and "…".
        RowLayout {
            spacing: Math.round(16 * Appearance.factor)
            Layout.fillWidth: true
            Layout.margins: Math.round(24 * Appearance.factor)
            Layout.bottomMargin: Math.round(16 * Appearance.factor)
            Image {
                source: itemDetails.item.icon ? "data:image/png;base64," + itemDetails.item.icon
                        : (itemDetails.item.defaultIcon || "")
                sourceSize: Qt.size(Math.round(40 * Appearance.factor), Math.round(40 * Appearance.factor))
                Layout.preferredWidth: Math.round(40 * Appearance.factor)
                Layout.preferredHeight: Math.round(40 * Appearance.factor)
                Layout.alignment: Qt.AlignTop
            }
            Label {
                objectName: "itemTitle"
                text: itemDetails.item.title || ""
                textFormat: Text.PlainText
                font.bold: true
                font.pointSize: Appearance.fontPointSize * 1.3
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
            }
            ToolButton {
                objectName: "editButton"
                visible: !itemDetails.item.trashed
                icon.source: "qrc:/icons/edit.svg"
                icon.color: itemDetails.palette.buttonText
                onClicked: itemDetails.editRequested()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Edit")
                Layout.alignment: Qt.AlignTop
            }
            ToolButton {
                objectName: "favouriteButton"
                icon.source: itemDetails.item.favourite ? "qrc:/icons/star-filled.svg" : "qrc:/icons/star.svg"
                icon.color: itemDetails.palette.buttonText
                onClicked: VaultController.setFavourite(itemDetails.itemId, !itemDetails.item.favourite)
                ToolTip.visible: hovered
                ToolTip.text: itemDetails.item.favourite ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
                Layout.alignment: Qt.AlignTop
            }
            ToolButton {
                objectName: "itemMenuButton"
                text: "…"
                onClicked: itemMenu.popup(this, 0, height)
                Layout.alignment: Qt.AlignTop
                Menu {
                    id: itemMenu
                    Action {
                        text: itemDetails.item.archived ? qsTr("Unarchive") : qsTr("Archive")
                        enabled: !itemDetails.item.trashed
                        onTriggered: itemDetails.archiveRequested()
                    }
                    Action {
                        text: itemDetails.item.trashed ? qsTr("Restore") : qsTr("Move to Trash")
                        onTriggered: itemDetails.item.trashed ? itemDetails.restoreRequested() : itemDetails.trashRequested()
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: itemDetails.palette.mid
        }

        ScrollView {
            id: fieldsScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: fieldsScroll.availableWidth - 2 * Math.round(24 * Appearance.factor)
                x: Math.round(24 * Appearance.factor)
                spacing: Math.round(12 * Appearance.factor)

                Item { Layout.preferredHeight: Math.round(12 * Appearance.factor) }

                // In the Trash or the Archive: said, with the way out.
                RowLayout {
                    visible: itemDetails.item.trashed === true || itemDetails.item.archived === true
                    Layout.fillWidth: true
                    Label {
                        text: itemDetails.item.trashed ? qsTr("This item is in the Trash.") : qsTr("This item is archived.")
                        font.bold: true
                        Layout.fillWidth: true
                    }
                    Button {
                        objectName: "restoreButton"
                        text: itemDetails.item.trashed ? qsTr("Restore") : qsTr("Unarchive")
                        onClicked: itemDetails.item.trashed ? itemDetails.restoreRequested() : itemDetails.archiveRequested()
                    }
                    Button {
                        objectName: "deleteForeverButton"
                        visible: itemDetails.item.trashed === true
                        text: qsTr("Delete Permanently…")
                        onClicked: itemDetails.trashRequested()
                    }
                }

                // What a merge left to settle (D-054), settled in the Sync
                // conflicts dialog (D-060).
                Rectangle {
                    id: mergeNote
                    objectName: "mergeNote"
                    visible: itemDetails.mergeNotes.length > 0
                    Layout.fillWidth: true
                    implicitHeight: noteLayout.implicitHeight + 2 * Math.round(10 * Appearance.factor)
                    radius: Math.round(6 * Appearance.factor)
                    color: "transparent"
                    border.color: "#d32f2f"
                    border.width: Math.max(1, Math.round(Appearance.factor))
                    RowLayout {
                        id: noteLayout
                        anchors.fill: parent
                        anchors.margins: Math.round(10 * Appearance.factor)
                        spacing: Math.round(8 * Appearance.factor)
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: qsTr("This item has a sync conflict.")
                        }
                        Button {
                            objectName: "settleConflictButton"
                            text: qsTr("Settle…")
                            onClicked: itemDetails.conflictRequested(itemDetails.itemId)
                        }
                    }
                }

                // The tags, each a way to the items that carry it (D-028).
                Flow {
                    Layout.fillWidth: true
                    spacing: Math.round(6 * Appearance.factor)
                    visible: (itemDetails.item.tags || []).length > 0
                    Repeater {
                        model: itemDetails.item.tags || []
                        delegate: Button {
                            required property string modelData
                            objectName: "detailsTag"
                            text: modelData
                            icon.source: "qrc:/icons/tag.svg"
                            icon.color: itemDetails.palette.buttonText
                            flat: true
                            onClicked: VaultController.filter = "tag:" + modelData
                        }
                    }
                }

                // One card per section, its heading above it at the value column.
                Repeater {
                    model: itemDetails.cards
                    delegate: ColumnLayout {
                        id: cardBlock
                        required property var modelData
                        spacing: Math.round(8 * Appearance.factor)
                        Layout.fillWidth: true
                        Label {
                            objectName: "sectionHeading"
                            visible: cardBlock.modelData.heading !== ""
                            text: cardBlock.modelData.heading
                            textFormat: Text.PlainText
                            font.bold: true
                            opacity: 0.7
                            leftPadding: itemDetails.labelWidth + Math.round(24 * Appearance.factor)
                            Layout.topMargin: Math.round(6 * Appearance.factor)
                        }
                        Rectangle {
                            objectName: "fieldCard"
                            Layout.fillWidth: true
                            implicitHeight: rows.implicitHeight
                            color: itemDetails.palette.base // the theme's Base (the owner, D-051)
                            border.color: itemDetails.lineColour
                            radius: Math.round(6 * Appearance.factor)
                            Column {
                                id: rows
                                width: parent.width
                                Repeater {
                                    model: cardBlock.modelData.rows
                                    delegate: FieldRow { details: itemDetails }
                                }
                            }
                        }
                    }
                }

                // The item's own note (D-033).
                Rectangle {
                    visible: (itemDetails.item.note || "") !== ""
                    Layout.fillWidth: true
                    implicitHeight: noteRow.implicitHeight
                    color: itemDetails.palette.base // the theme's Base (the owner, D-051)
                    border.color: itemDetails.lineColour
                    radius: Math.round(6 * Appearance.factor)
                    FieldRow {
                        id: noteRow
                        details: itemDetails
                        modelData: ({ name: qsTr("Note"), kind: "multiline", value: itemDetails.item.note || "",
                                      sensitive: false, history: [] })
                        index: 0
                    }
                }

                // The attachments (D-029): saved only where the person chooses.
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: attachmentRows.implicitHeight
                    visible: (itemDetails.item.attachments || []).length > 0
                    color: itemDetails.palette.base // the theme's Base (the owner, D-051)
                    border.color: itemDetails.lineColour
                    radius: Math.round(6 * Appearance.factor)
                    Column {
                        id: attachmentRows
                        width: parent.width
                        Repeater {
                            model: itemDetails.item.attachments || []
                            delegate: Item {
                                id: attachmentRow
                                required property var modelData
                                required property int index
                                width: attachmentRows.width
                                implicitHeight: itemDetails.rowHeight
                                Rectangle {
                                    visible: attachmentRow.index > 0
                                    width: parent.width
                                    height: 1
                                    color: itemDetails.lineColour
                                }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Math.round(16 * Appearance.factor)
                                    anchors.rightMargin: Math.round(8 * Appearance.factor)
                                    spacing: Math.round(10 * Appearance.factor)
                                    Image {
                                        source: "qrc:/icons/attachment.svg"
                                        sourceSize: Qt.size(Math.round(16 * Appearance.factor), Math.round(16 * Appearance.factor))
                                    }
                                    Label {
                                        text: attachmentRow.modelData.name
                                        textFormat: Text.PlainText
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: VaultController.sizeText(attachmentRow.modelData.size)
                                        opacity: 0.7
                                    }
                                    Button {
                                        objectName: "saveAttachmentButton"
                                        text: qsTr("Save as…")
                                        onClicked: itemDetails.askWhereToSave(attachmentRow.modelData)
                                    }
                                }
                            }
                        }
                    }
                }

                // When it was changed and made, and Autosubmit Login (D-033).
                GridLayout {
                    columns: 2
                    Layout.fillWidth: true
                    columnSpacing: Math.round(24 * Appearance.factor)
                    rowSpacing: Math.round(8 * Appearance.factor)
                    Layout.topMargin: Math.round(16 * Appearance.factor)
                    Label {
                        text: qsTr("Last Modified:")
                        opacity: 0.7
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: itemDetails.labelWidth
                    }
                    Label {
                        objectName: "lastModified"
                        text: itemDetails.item.modified || ""
                        opacity: 0.85
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: qsTr("Created:")
                        opacity: 0.7
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: itemDetails.labelWidth
                    }
                    Label {
                        text: itemDetails.item.created || ""
                        opacity: 0.85
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Item { Layout.preferredWidth: itemDetails.labelWidth; visible: autoSubmitBox.visible }
                    CheckBox {
                        id: autoSubmitBox
                        objectName: "autoSubmitBox"
                        visible: itemDetails.item.categoryId === "login" && !itemDetails.item.trashed
                        text: qsTr("Autosubmit Login")
                        checked: itemDetails.item.autoSubmit === true
                        onToggled: {
                            let changed = VaultController.item(itemDetails.itemId)
                            changed.autoSubmit = checked
                            VaultController.saveItem(changed)
                        }
                    }
                }

                Item { Layout.preferredHeight: Math.round(24 * Appearance.factor) }
            }
        }
    }

    function showHistory(field) {
        historyWindow.showFor(field)
    }

    // A field's earlier values, newest first (D-033).
    DialogWindow {
        id: historyWindow
        objectName: "historyWindow"
        memoryName: "fieldHistory"
        property var field: ({})
        property bool revealed: false
        title: qsTr("History of “%1”").arg(field.name || "")
        standardButtons: Dialog.Close
        function showFor(chosen) {
            field = chosen
            revealed = false
            open()
        }
        leftButtons: Button {
            visible: historyWindow.field.sensitive === true
            text: historyWindow.revealed ? qsTr("Hide") : qsTr("Show")
            onClicked: historyWindow.revealed = !historyWindow.revealed
        }
        ColumnLayout {
            spacing: Math.round(6 * Appearance.factor)
            Layout.preferredWidth: Math.round(460 * Appearance.factor)
            Layout.fillWidth: true
            Repeater {
                model: historyWindow.field.history || []
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Label {
                        text: historyWindow.field.sensitive && !historyWindow.revealed ? "••••••••" : modelData.value
                        textFormat: Text.PlainText
                        font.family: "Noto Sans Mono"
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: modelData.changed
                        opacity: 0.7
                    }
                    Button {
                        text: qsTr("Copy")
                        onClicked: VaultController.copyToClipboard(modelData.value)
                    }
                }
            }
        }
    }

    // A web or email address as a link, its text escaped, never read as markup.
    function link(address, words) {
        const escape = function(text) {
            return String(text).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;")
        }
        return "<a style=\"text-decoration:none\" href=\"" + escape(address) + "\">" + escape(words) + "</a>"
    }

    property var savingAttachment: ({})
    function askWhereToSave(attachment) {
        savingAttachment = attachment
        saveDialog.selectedFile = saveDialog.currentFolder + "/" + encodeURIComponent(attachment.name)
        saveDialog.open()
    }

    FilePicker {
        id: saveDialog
        memoryName: "ItemDetails.saveDialog"
        parentWindow: itemDetails
        title: qsTr("Save the attachment")
        fileMode: FileDialog.SaveFile
        onAccepted: VaultController.saveAttachment(itemDetails.itemId, itemDetails.savingAttachment.id, selectedFile)
    }
}
