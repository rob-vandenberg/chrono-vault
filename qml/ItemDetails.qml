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
// ItemDetails.qml 0.0.9

// --- Version history ----------------------------------------------------
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
        anchors.margins: 16
        spacing: 12

        RowLayout {
            spacing: 12
            Layout.fillWidth: true
            Image {
                source: itemDetails.item.icon ? "data:image/png;base64," + itemDetails.item.icon
                        : (itemDetails.item.isNote ? "qrc:/icons/item-note.svg" : "qrc:/icons/item-login.svg")
                sourceSize: Qt.size(48, 48)
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
            }
            Label {
                objectName: "itemTitle"
                text: itemDetails.item.title || ""
                textFormat: Text.PlainText
                font.bold: true
                font.pointSize: Appearance.fontPointSize * 1.4
                elide: Text.ElideRight
                Layout.fillWidth: true
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

        ScrollView {
            id: fieldsScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            Rectangle {
                width: fieldsScroll.availableWidth
                implicitHeight: rows.implicitHeight
                visible: itemDetails.shownFields.length > 0
                color: itemDetails.palette.base
                border.color: itemDetails.palette.mid
                radius: 4

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
                            implicitHeight: Math.max(44, line.implicitHeight + 20)

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
                                anchors.leftMargin: 12
                                anchors.rightMargin: 6
                                spacing: 16

                                Label {
                                    text: row.modelData.name
                                    textFormat: Text.PlainText
                                    horizontalAlignment: Text.AlignRight
                                    opacity: 0.7
                                    elide: Text.ElideRight
                                    Layout.preferredWidth: 140
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
                                    Layout.preferredWidth: 26
                                    Layout.preferredHeight: 26
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
        }
    }

    // A web address as a link, its text escaped, never read as markup.
    function link(address) {
        const escaped = String(address).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;")
        return "<a href=\"" + escaped + "\">" + escaped + "</a>"
    }
}
