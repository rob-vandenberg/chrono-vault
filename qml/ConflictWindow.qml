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
// ConflictWindow.qml 0.5.76

// --- Version history ----------------------------------------------------
// v0.5.76: Initial release (D-060)

// One item's conflict, after Chrono Mail's (D-060): every value that differs
// with the value now and the other value, outlined, a choice in front of each;
// the header's choices take one side for every row. Nothing is chosen
// beforehand: Apply waits until every row is chosen. Keep both keeps the item
// and a copy with the other values. An item deleted on one side and changed
// on the other is kept or moved to the Trash.
DialogWindow {
    id: conflictWindow
    objectName: "conflictWindow"
    memoryName: "conflict"

    property string itemId: ""
    property var conflict: ({})
    readonly property var rows: conflict.rows || []
    readonly property bool deletedElsewhere: conflict.kind === "deleted"
    // Each row's choice by its index: "now" or "other".
    property var chosen: ({})
    property bool reveal: false
    readonly property bool allChosen: {
        for (let i = 0; i < rows.length; ++i) {
            if (chosen[rows[i].index] === undefined)
                return false
        }
        return rows.length > 0
    }
    function allAre(side) {
        if (rows.length === 0)
            return false
        for (let i = 0; i < rows.length; ++i) {
            if (chosen[rows[i].index] !== side)
                return false
        }
        return true
    }
    function choose(index, side) {
        const next = Object.assign({}, chosen)
        next[index] = side
        chosen = next
    }
    function chooseAll(side) {
        const next = {}
        for (let i = 0; i < rows.length; ++i)
            next[rows[i].index] = side
        chosen = next
    }
    function showItem(id) {
        itemId = id
        conflict = VaultController.conflictOf(id)
        chosen = {}
        reveal = false
        open()
    }
    function apply() {
        const choices = []
        for (let i = 0; i < rows.length; ++i)
            choices[rows[i].index] = chosen[rows[i].index]
        for (let j = 0; j < choices.length; ++j) {
            if (choices[j] === undefined)
                choices[j] = ""
        }
        VaultController.settleConflict(itemId, choices)
        accept()
    }
    function shown_(text, hidden) {
        return hidden && !reveal ? "••••••••" : text
    }

    title: qsTr("Sync conflict: %1").arg(conflict.name || "")
    standardButtons: Dialog.Cancel

    leftButtons: Button {
        objectName: "conflictKeepBothButton"
        visible: !conflictWindow.deletedElsewhere
        text: qsTr("Keep both")
        onClicked: {
            VaultController.settleConflicts([conflictWindow.itemId], "both")
            conflictWindow.accept()
        }
    }
    rightButtons: [
        Button {
            objectName: "conflictApplyButton"
            visible: !conflictWindow.deletedElsewhere
            text: qsTr("Apply")
            enabled: conflictWindow.allChosen
            highlighted: true
            onClicked: conflictWindow.apply()
        },
        Button {
            objectName: "conflictKeepButton"
            visible: conflictWindow.deletedElsewhere
            text: qsTr("Keep")
            onClicked: {
                VaultController.settleConflicts([conflictWindow.itemId], "now")
                conflictWindow.accept()
            }
        },
        Button {
            objectName: "conflictTrashButton"
            visible: conflictWindow.deletedElsewhere
            text: qsTr("Move to Trash")
            onClicked: {
                VaultController.settleConflicts([conflictWindow.itemId], "other")
                conflictWindow.accept()
            }
        }
    ]

    // A settled or vanished conflict (a sync, a lock) closes the window.
    Connections {
        target: VaultController
        function onRevisionChanged() {
            if (!conflictWindow.visible)
                return
            const now = VaultController.conflictOf(conflictWindow.itemId)
            if (now.itemId === undefined)
                conflictWindow.reject()
        }
    }

    ColumnLayout {
        spacing: Math.round(8 * Appearance.factor)
        Layout.preferredWidth: Math.round(620 * Appearance.factor)
        Layout.fillWidth: true
        Layout.fillHeight: true

        GridLayout {
            columns: 2
            columnSpacing: Math.round(12 * Appearance.factor)
            rowSpacing: Math.round(2 * Appearance.factor)
            Label { text: qsTr("What:"); opacity: 0.7 }
            Label { text: conflictWindow.conflict.what || ""; textFormat: Text.PlainText }
            Label { text: qsTr("Where:"); opacity: 0.7 }
            Label { text: conflictWindow.conflict.where || ""; textFormat: Text.PlainText }
            Label { text: qsTr("Why:"); opacity: 0.7 }
            Label {
                text: conflictWindow.deletedElsewhere
                      ? qsTr("Deleted on one side and changed on the other, so it was kept.")
                      : qsTr("Changed on two sides. The newest value stands until you choose.")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        CheckBox {
            objectName: "conflictRevealBox"
            visible: !conflictWindow.deletedElsewhere
            text: qsTr("Show hidden values")
            checked: conflictWindow.reveal
            onToggled: conflictWindow.reveal = checked
        }

        ScrollView {
            visible: !conflictWindow.deletedElsewhere
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Math.min(choices.implicitHeight, Math.round(420 * Appearance.factor))
            contentWidth: availableWidth
            clip: true

            GridLayout {
                id: choices
                width: parent ? parent.width : 0
                columns: 3
                columnSpacing: Math.round(10 * Appearance.factor)
                rowSpacing: Math.round(6 * Appearance.factor)

                Label { text: qsTr("Field"); font.bold: true }
                RadioButton {
                    objectName: "conflictAllNow"
                    text: qsTr("Value now")
                    font.bold: true
                    autoExclusive: false
                    // Shown as the window's choice only: a click never ticks it itself.
                    checkable: false
                    checked: conflictWindow.allAre("now")
                    onClicked: conflictWindow.chooseAll("now")
                    Layout.fillWidth: true
                }
                RadioButton {
                    objectName: "conflictAllOther"
                    text: qsTr("Other value")
                    font.bold: true
                    autoExclusive: false
                    // Shown as the window's choice only: a click never ticks it itself.
                    checkable: false
                    checked: conflictWindow.allAre("other")
                    onClicked: conflictWindow.chooseAll("other")
                    Layout.fillWidth: true
                }

                Repeater {
                    model: conflictWindow.rows
                    delegate: Label {
                        required property var modelData
                        required property int index
                        Layout.row: index + 1
                        Layout.column: 0
                        text: modelData.label
                        textFormat: Text.PlainText
                        Layout.alignment: Qt.AlignTop
                    }
                }
                Repeater {
                    model: conflictWindow.rows
                    delegate: Frame {
                        id: nowCell
                        required property var modelData
                        required property int index
                        Layout.row: index + 1
                        Layout.column: 1
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        padding: Math.round(2 * Appearance.factor)
                        background: Rectangle {
                            color: "transparent"
                            radius: Math.round(4 * Appearance.factor)
                            border.width: Math.max(2, Math.round(2 * Appearance.factor))
                            border.color: conflictWindow.palette.highlight
                        }
                        RadioButton {
                            objectName: "conflictNow_" + nowCell.index
                            width: parent.width
                            autoExclusive: false
                            // Shown as the window's choice only: a click never ticks it itself.
                            checkable: false
                            text: conflictWindow.shown_(nowCell.modelData.now, nowCell.modelData.hidden)
                            checked: conflictWindow.chosen[nowCell.modelData.index] === "now"
                            onClicked: conflictWindow.choose(nowCell.modelData.index, "now")
                        }
                    }
                }
                Repeater {
                    model: conflictWindow.rows
                    delegate: Frame {
                        id: otherCell
                        required property var modelData
                        required property int index
                        Layout.row: index + 1
                        Layout.column: 2
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        padding: Math.round(2 * Appearance.factor)
                        background: Rectangle {
                            color: "transparent"
                            radius: Math.round(4 * Appearance.factor)
                            border.width: Math.max(2, Math.round(2 * Appearance.factor))
                            border.color: conflictWindow.palette.highlight
                        }
                        RadioButton {
                            objectName: "conflictOther_" + otherCell.index
                            width: parent.width
                            autoExclusive: false
                            // Shown as the window's choice only: a click never ticks it itself.
                            checkable: false
                            text: conflictWindow.shown_(otherCell.modelData.other, otherCell.modelData.hidden)
                            checked: conflictWindow.chosen[otherCell.modelData.index] === "other"
                            onClicked: conflictWindow.choose(otherCell.modelData.index, "other")
                        }
                    }
                }
            }
        }
    }
}
