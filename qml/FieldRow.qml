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

// --- Version ------------------------------------------------------------
// FieldRow.qml 0.6.100

// --- Version history ----------------------------------------------------
// v0.6.100: Padlocks and Open in Settings > Vaults; round picture buttons (D-071)
// v0.6.92: Exact sizes (D-069)
// v0.6.86: What the audit says under a password (D-065)
// v0.5.82: A double click copies, a right click opens the menu (D-062)
// v0.0.24: A copy button for the one-time code; Item > Show Passwords (D-039)
// v0.0.17: Initial release (D-033)

// One field's row in an item's card (D-033): the label at the right in grey,
// the value beside it (as dots while hidden, a link for a web or email
// address), a one-time code with its ring, and the "…" menu.
Item {
    id: row
    required property var details  // the item's view, for its sizes, colours and windows
    required property var modelData
    required property int index
    property bool revealed: false
    readonly property string kind: modelData.kind
    // Shown as text by its own Show, or by Item > Show Passwords (D-039).
    readonly property bool shownPlain: revealed || details.revealAll
    readonly property bool isSecret: modelData.sensitive === true && VaultController.hideSensitive && !shownPlain
    readonly property bool isLink: (kind === "url" && /^https?:\/\//i.test(modelData.value)) || kind === "email"
    readonly property string code: { details.tick; return kind === "totp" ? VaultController.oneTimeCode(modelData.value) : "" }
    readonly property int secondsLeft: { details.tick; return kind === "totp" ? VaultController.oneTimeCodeSecondsLeft(modelData.value) : 0 }
    width: parent ? parent.width : 0
    // What the audit says of a password (D-065).
    readonly property var remarks: kind === "password" && VaultController.audit
                                   ? VaultController.auditRemarks(details.itemId, modelData.uid || 0) : []
    readonly property real remarksHeight: remarks.length > 0 ? remarkColumn.implicitHeight + 4 * Appearance.factor : 0
    implicitHeight: Math.max(details.rowHeight, valueText.implicitHeight + remarksHeight + 30 * Appearance.factor)

    HoverHandler { id: hover }
    readonly property string toCopy: kind === "totp" ? code : modelData.value
    // A double click copies the value, a right click opens the "…" menu
    // (D-062); a link opens with its own click.
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onDoubleTapped: if (valueText.hoveredLink === "") VaultController.copyToClipboard(row.toCopy)
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: rowMenu.popup()
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Math.round(5 * Appearance.factor)
        color: details.palette.highlight
        opacity: hover.hovered ? 0.08 : 0
    }
    Rectangle {
        visible: row.index > 0
        width: parent.width
        height: 1
        color: details.lineColour
    }
    Label {
        text: row.modelData.name
        textFormat: Text.PlainText
        horizontalAlignment: Text.AlignRight
        opacity: 0.7
        elide: Text.ElideRight
        width: details.labelWidth
        anchors.verticalCenter: parent.verticalCenter
    }
    Label {
        id: valueText
        objectName: "fieldValue"
        x: details.labelWidth + 24 * Appearance.factor
        width: parent.width - x - 90 * Appearance.factor
        anchors.verticalCenter: row.kind === "multiline" ? undefined : parent.verticalCenter
        anchors.verticalCenterOffset: -row.remarksHeight / 2
        y: row.kind === "multiline" ? 15 * Appearance.factor : 0
        text: row.isSecret ? "••••••••"
              : row.kind === "totp" ? (row.code.length === 6 ? row.code.slice(0, 3) + " " + row.code.slice(3) : row.code)
              : row.isLink ? details.link(row.kind === "email" ? "mailto:" + row.modelData.value : row.modelData.value,
                                              row.modelData.value)
              : row.modelData.value
        textFormat: row.isLink && !row.isSecret ? Text.RichText : Text.PlainText
        linkColor: details.palette.link
        font.family: row.modelData.sensitive && row.shownPlain ? "Noto Sans Mono" : Appearance.font.family
        wrapMode: Text.Wrap
        onLinkActivated: function(address) { Qt.openUrlExternally(address) }
        HoverHandler {
            enabled: valueText.textFormat === Text.RichText
            cursorShape: Qt.PointingHandCursor
        }
    }
    Column {
        id: remarkColumn
        objectName: "auditRemarks"
        visible: row.remarks.length > 0
        x: valueText.x
        width: valueText.width
        anchors.top: valueText.bottom
        anchors.topMargin: 4 * Appearance.factor
        spacing: 2 * Appearance.factor
        Repeater {
            model: row.remarks
            delegate: Label {
                id: remark
                required property var modelData
                readonly property bool opens: modelData.kind === "reused"
                width: remarkColumn.width
                readonly property string address: "items"
                text: opens ? details.link(address, modelData.text) : modelData.text
                textFormat: opens ? Text.RichText : Text.PlainText
                color: details.palette.window.hslLightness < 0.5 ? "#ff6b6b" : "#c62828"
                linkColor: color
                wrapMode: Text.Wrap
                onLinkActivated: sameMenu.popup()
                HoverHandler {
                    enabled: remark.opens
                    cursorShape: Qt.PointingHandCursor
                }
                // The other items with this password.
                Menu {
                    id: sameMenu
                    Instantiator {
                        model: remark.modelData.items || []
                        delegate: MenuItem {
                            required property var modelData
                            text: modelData.title
                            onTriggered: details.itemRequested(modelData.id)
                        }
                        onObjectAdded: function(index, object) { sameMenu.insertItem(index, object) }
                        onObjectRemoved: function(index, object) { sameMenu.removeItem(object) }
                    }
                }
            }
        }
        AuditSource {
            visible: row.remarks.some(function(one) { return one.kind === "leaked" || one.kind === "breached" })
        }
    }
    // The one-time code copied with one click, left of its ring (D-039).
    PictureButton {
        objectName: "copyCodeButton"
        visible: row.kind === "totp" && row.code !== ""
        icon.source: "qrc:/icons/copy.svg"
        icon.color: details.palette.buttonText
        anchors.right: ring.left
        anchors.rightMargin: 6 * Appearance.factor
        anchors.verticalCenter: parent.verticalCenter
        onClicked: VaultController.copyToClipboard(row.code)
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Copy")
    }
    // The seconds the one-time code has left, as a ring, red at the end.
    Item {
        id: ring
        visible: row.kind === "totp" && row.code !== ""
        width: 26 * Appearance.factor
        height: width
        anchors.right: menuButton.left
        anchors.rightMargin: 10 * Appearance.factor
        anchors.verticalCenter: parent.verticalCenter
        Canvas {
            anchors.fill: parent
            readonly property int seconds: row.secondsLeft
            onSecondsChanged: requestPaint()
            onPaint: {
                const context = getContext("2d")
                context.reset()
                context.lineWidth = 2
                context.strokeStyle = details.palette.mid
                context.beginPath()
                context.arc(width / 2, height / 2, width / 2 - 2, 0, 2 * Math.PI)
                context.stroke()
                context.strokeStyle = seconds <= 5 ? "#e53e3e" : "#38a169"
                context.beginPath()
                context.arc(width / 2, height / 2, width / 2 - 2, -Math.PI / 2, -Math.PI / 2 + 2 * Math.PI * seconds / 30)
                context.stroke()
            }
        }
        Label {
            anchors.centerIn: parent
            text: row.secondsLeft
            font.pointSize: Appearance.fontPointSize * 0.7
        }
    }
    PictureButton {
        id: menuButton
        objectName: "rowMenuButton"
        dots: true
        menuOpen: rowMenu.visible
        anchors.right: parent.right
        anchors.rightMargin: 8 * Appearance.factor
        anchors.verticalCenter: parent.verticalCenter
        onClicked: rowMenu.popup(this, 0, height)
        Menu {
            id: rowMenu
            Action {
                text: qsTr("Copy")
                onTriggered: VaultController.copyToClipboard(row.toCopy)
            }
            MenuItem {
                text: row.revealed ? qsTr("Hide") : qsTr("Show")
                visible: row.modelData.sensitive === true && VaultController.hideSensitive
                height: visible ? implicitHeight : 0
                onTriggered: row.revealed = !row.revealed
            }
            MenuItem {
                objectName: "historyItem"
                text: qsTr("History")
                visible: (row.modelData.history || []).length > 0
                height: visible ? implicitHeight : 0
                onTriggered: details.showHistory(row.modelData)
            }
        }
    }
}
