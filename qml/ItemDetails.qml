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
// ItemDetails.qml 0.0.14

// --- Version history ----------------------------------------------------
// v0.0.14: The icon of the item's category; the star that marks a favourite; the template under the title; the
//          tags; the attachments, each saved with Save as… (D-027, D-028, D-029)
// v0.0.9: Initial release (D-025)

// The chosen item, read-only (D-025): its icon and title with the pencil
// that opens the editor, then its fields in a framed card: the label at the
// right in grey, the value beside it, a "…" menu at the end of each row.
Item {
    id: itemDetails
    objectName: "itemDetails"

    property string itemId: ""
    // Read again whenever the vault's items change.
    property int refresh: 0
    readonly property var item: itemId !== "" && refresh >= 0 ? VaultController.item(itemId) : ({})
    readonly property var shownFields: {
        const all = item.fields || []
        return all.filter(function(field) { return field.value !== "" })
    }

    signal editRequested()

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
        anchors.margins: Math.round(16 * Appearance.factor)
        spacing: Math.round(12 * Appearance.factor)

        RowLayout {
            spacing: Math.round(12 * Appearance.factor)
            Layout.fillWidth: true
            Image {
                source: itemDetails.item.icon ? "data:image/png;base64," + itemDetails.item.icon
                        : (itemDetails.item.defaultIcon || "")
                sourceSize: Qt.size(Math.round(48 * Appearance.factor), Math.round(48 * Appearance.factor))
                Layout.preferredWidth: Math.round(48 * Appearance.factor)
                Layout.preferredHeight: Math.round(48 * Appearance.factor)
            }
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                Label {
                    objectName: "itemTitle"
                    text: itemDetails.item.title || ""
                    textFormat: Text.PlainText
                    font.bold: true
                    font.pointSize: Appearance.fontPointSize * 1.4
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: itemDetails.item.templateName || ""
                    textFormat: Text.PlainText
                    opacity: 0.7
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            ToolButton {
                objectName: "favouriteButton"
                icon.source: itemDetails.item.favourite ? "qrc:/icons/star-filled.svg" : "qrc:/icons/star.svg"
                icon.color: itemDetails.palette.buttonText
                onClicked: VaultController.setFavourite(itemDetails.itemId, !itemDetails.item.favourite)
                ToolTip.visible: hovered
                ToolTip.text: itemDetails.item.favourite ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
            }
            ToolButton {
                objectName: "editButton"
                icon.source: "qrc:/icons/edit.svg"
                icon.color: itemDetails.palette.buttonText
                onClicked: itemDetails.editRequested()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Edit")
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

        ScrollView {
            id: fieldsScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            Column {
                width: fieldsScroll.availableWidth
                spacing: Math.round(12 * Appearance.factor)

            Rectangle {
                width: parent.width
                implicitHeight: rows.implicitHeight
                visible: itemDetails.shownFields.length > 0
                color: itemDetails.palette.base
                border.color: itemDetails.palette.mid
                radius: Math.round(4 * Appearance.factor)

                Column {
                    id: rows
                    width: parent.width

                    Repeater {
                        model: itemDetails.shownFields
                        delegate: Item {
                            id: row
                            required property var modelData
                            required property int index
                            property bool revealed: false
                            readonly property string kind: modelData.kind
                            readonly property string code: {
                                itemDetails.tick
                                return kind === "totp" ? VaultController.oneTimeCode(modelData.value) : ""
                            }
                            readonly property int secondsLeft: {
                                itemDetails.tick
                                return kind === "totp" ? VaultController.oneTimeCodeSecondsLeft(modelData.value) : 0
                            }
                            width: rows.width
                            implicitHeight: Math.max(Math.round(44 * Appearance.factor), line.implicitHeight + Math.round(20 * Appearance.factor))

                            Rectangle {
                                visible: row.index > 0
                                width: parent.width
                                height: 1
                                color: itemDetails.palette.mid
                            }

                            RowLayout {
                                id: line
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: Math.round(12 * Appearance.factor)
                                anchors.rightMargin: Math.round(6 * Appearance.factor)
                                spacing: Math.round(16 * Appearance.factor)

                                Label {
                                    text: row.modelData.name
                                    textFormat: Text.PlainText
                                    horizontalAlignment: Text.AlignRight
                                    opacity: 0.7
                                    elide: Text.ElideRight
                                    Layout.preferredWidth: Math.round(140 * Appearance.factor)
                                    // Beside the value; at the top of a text of several lines.
                                    Layout.alignment: row.kind === "multiline" ? Qt.AlignTop : Qt.AlignVCenter
                                }
                                Label {
                                    objectName: "fieldValue"
                                    readonly property bool isLink: row.kind === "url"
                                        && /^https?:\/\//i.test(row.modelData.value)
                                    text: row.kind === "password" && !row.revealed ? "••••••••"
                                          : row.kind === "totp" ? (row.code.length === 6 ? row.code.slice(0, 3) + " " + row.code.slice(3) : row.code)
                                          : isLink ? itemDetails.link(row.modelData.value)
                                          : row.modelData.value
                                    textFormat: isLink ? Text.StyledText : Text.PlainText
                                    linkColor: itemDetails.palette.link
                                    font.family: row.kind === "password" && row.revealed ? "Noto Sans Mono" : Appearance.font.family
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                    onLinkActivated: Qt.openUrlExternally(row.modelData.value)
                                    HoverHandler {
                                        enabled: parent.isLink
                                        cursorShape: Qt.PointingHandCursor
                                    }
                                }
                                // The seconds the one-time code has left, as a ring.
                                Item {
                                    visible: row.kind === "totp"
                                    Layout.preferredWidth: Math.round(26 * Appearance.factor)
                                    Layout.preferredHeight: Math.round(26 * Appearance.factor)
                                    Canvas {
                                        id: ring
                                        anchors.fill: parent
                                        readonly property int seconds: row.secondsLeft
                                        onSecondsChanged: requestPaint()
                                        onPaint: {
                                            const context = getContext("2d")
                                            context.reset()
                                            context.lineWidth = 2
                                            context.strokeStyle = itemDetails.palette.mid
                                            context.beginPath()
                                            context.arc(width / 2, height / 2, width / 2 - 2, 0, 2 * Math.PI)
                                            context.stroke()
                                            context.strokeStyle = itemDetails.palette.highlight
                                            context.beginPath()
                                            context.arc(width / 2, height / 2, width / 2 - 2, -Math.PI / 2,
                                                        -Math.PI / 2 + 2 * Math.PI * seconds / 30)
                                            context.stroke()
                                        }
                                    }
                                    Label {
                                        anchors.centerIn: parent
                                        text: row.secondsLeft
                                        font.pointSize: Appearance.fontPointSize * 0.7
                                    }
                                }
                                ToolButton {
                                    objectName: "rowMenuButton"
                                    text: "…"
                                    Layout.alignment: row.kind === "multiline" ? Qt.AlignTop : Qt.AlignVCenter
                                    onClicked: rowMenu.popup(this, 0, height)
                                    Menu {
                                        id: rowMenu
                                        Action {
                                            text: qsTr("Copy")
                                            onTriggered: VaultController.copyToClipboard(
                                                             row.kind === "totp" ? row.code : row.modelData.value)
                                        }
                                        // Only a password has something to show.
                                        MenuItem {
                                            text: row.revealed ? qsTr("Hide") : qsTr("Show")
                                            visible: row.kind === "password"
                                            height: visible ? implicitHeight : 0
                                            onTriggered: row.revealed = !row.revealed
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // The attachments (D-029): saved only where the person chooses,
            // never opened from a copy left on the disk.
            Rectangle {
                width: parent.width
                implicitHeight: attachmentRows.implicitHeight
                visible: (itemDetails.item.attachments || []).length > 0
                color: itemDetails.palette.base
                border.color: itemDetails.palette.mid
                radius: Math.round(4 * Appearance.factor)

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
                            implicitHeight: Math.round(44 * Appearance.factor)
                            Rectangle {
                                visible: attachmentRow.index > 0
                                width: parent.width
                                height: 1
                                color: itemDetails.palette.mid
                            }
                            RowLayout {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: Math.round(12 * Appearance.factor)
                                anchors.rightMargin: Math.round(6 * Appearance.factor)
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
            }
        }
    }

    // A web address as a link, its text escaped, never read as markup.
    function link(address) {
        const escaped = String(address).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;")
        return "<a href=\"" + escaped + "\">" + escaped + "</a>"
    }

    property var savingAttachment: ({})
    function askWhereToSave(attachment) {
        savingAttachment = attachment
        saveDialog.selectedFile = saveDialog.currentFolder + "/" + encodeURIComponent(attachment.name)
        saveDialog.open()
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save the attachment")
        fileMode: FileDialog.SaveFile
        onAccepted: VaultController.saveAttachment(itemDetails.itemId, itemDetails.savingAttachment.id, selectedFile)
    }
}
