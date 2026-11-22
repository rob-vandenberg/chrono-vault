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
// FieldRow.qml 0.0.24

// --- Version history ----------------------------------------------------
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
    implicitHeight: Math.max(details.rowHeight, valueText.implicitHeight + Math.round(30 * Appearance.factor))

    HoverHandler { id: hover }
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
        x: details.labelWidth + Math.round(24 * Appearance.factor)
        width: parent.width - x - Math.round(90 * Appearance.factor)
        anchors.verticalCenter: row.kind === "multiline" ? undefined : parent.verticalCenter
        y: row.kind === "multiline" ? Math.round(15 * Appearance.factor) : 0
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
    // The one-time code copied with one click, left of its ring (D-039).
    ToolButton {
        objectName: "copyCodeButton"
        visible: row.kind === "totp" && row.code !== ""
        icon.source: "qrc:/icons/copy.svg"
        icon.color: details.palette.buttonText
        anchors.right: ring.left
        anchors.rightMargin: Math.round(6 * Appearance.factor)
        anchors.verticalCenter: parent.verticalCenter
        onClicked: VaultController.copyToClipboard(row.code)
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Copy")
    }
    // The seconds the one-time code has left, as a ring, red at the end.
    Item {
        id: ring
        visible: row.kind === "totp" && row.code !== ""
        width: Math.round(26 * Appearance.factor)
        height: width
        anchors.right: menuButton.left
        anchors.rightMargin: Math.round(10 * Appearance.factor)
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
    ToolButton {
        id: menuButton
        objectName: "rowMenuButton"
        text: "…"
        anchors.right: parent.right
        anchors.rightMargin: Math.round(8 * Appearance.factor)
        anchors.verticalCenter: parent.verticalCenter
        onClicked: rowMenu.popup(this, 0, height)
        Menu {
            id: rowMenu
            Action {
                text: qsTr("Copy")
                onTriggered: VaultController.copyToClipboard(row.kind === "totp" ? row.code : row.modelData.value)
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
