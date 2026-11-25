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
// ConflictWindow.qml 0.6.94

// --- Version history ----------------------------------------------------
// v0.6.94: The main window keeps its size, the others scale along (D-070)
// v0.6.92: Exact sizes (D-069)
// v0.5.82: The normal scroll bar, Enter applies; the split button with 5 pixels free at its sides (D-062)
// v0.5.81: As Chrono Mail's: every field, the choices in the second accent, a split, Retry sync, Keep here only and Keep both chosen and applied (D-061)
// v0.5.78: The sides "Vault" and "Import", or "This device" and "Other device"; every kind of difference; the header's choices in line with the rows' (D-061)
// v0.5.77: A conflict from an import reads "Imported" (D-060)
// v0.5.76: Initial release (D-060)

// One item's conflict, as Chrono Mail's (D-060, D-061): every field of the
// item with its two sides, the vault and the import or this device and the
// other device. What is the same is plain; what differs is outlined, a choice
// in front of each side. The header's choices take one side for every row.
// Nothing is chosen beforehand: Apply waits until every row is chosen or
// split, or a whole way is chosen. An item deleted on one side is chosen as a
// whole, with the header's choices.
DialogWindow {
    id: conflictWindow
    objectName: "conflictWindow"
    memoryName: "conflict"

    property string itemId: ""
    property var conflict: ({})
    // What is to be chosen, and every field of the item in its order.
    readonly property var rows: conflict.rows || []
    readonly property var allRows: conflict.allRows || []
    readonly property string kind: conflict.kind || ""
    // Each row's choice by its index: "left" or "right".
    property var chosen: ({})
    // The rows split into both values, in the order they were split, and the
    // choice each had before.
    property var split: []
    property var beforeSplit: ({})
    // "keep" (Keep here only), "retry" (Retry sync) or "both" (Keep both).
    property string way: ""
    // The side of an item deleted on one side.
    property string wholeSide: ""
    property bool reveal: false
    // The size it opened with: another at closing is the person's.
    property size openedSize: Qt.size(0, 0)
    readonly property int openRows: 11

    // Chrono Mail's second accent: what differs in a sync conflict.
    readonly property color accent: palette.window.hslLightness < 0.5 ? "#c86400" : "#c62828"
    readonly property int line: Math.max(2, Math.round(2 * Appearance.factor))
    readonly property real innerSide: 4 * Appearance.factor
    readonly property real innerTop: 2 * Appearance.factor
    readonly property int circleSize: Math.round(14 * Appearance.factor)
    readonly property real circleGap: 6 * Appearance.factor
    // Where the text beside a circle begins.
    readonly property int textLeft: line + innerSide + circleSize + circleGap

    readonly property var openChoices: rows.filter(function(row) { return split.indexOf(row.index) < 0 })
    readonly property bool ready: {
        if (way !== "")
            return true
        if (rows.length === 0)
            return wholeSide !== ""
        return openChoices.every(function(row) { return chosen[row.index] !== undefined })
    }
    readonly property bool anyHidden: allRows.some(function(row) { return row.leftHidden || row.rightHidden })

    function allAre(side) {
        if (rows.length === 0)
            return wholeSide === side
        return way === "" && openChoices.length > 0
                && openChoices.every(function(row) { return chosen[row.index] === side })
    }
    function choose(index, side) {
        const next = Object.assign({}, chosen)
        next[index] = side
        chosen = next
        way = "" // the fields decide again
    }
    function chooseAll(side) {
        way = ""
        if (rows.length === 0) {
            wholeSide = side
            return
        }
        const next = {}
        for (let i = 0; i < openChoices.length; ++i)
            next[openChoices[i].index] = side
        chosen = next
    }
    // A way that settles the item as a whole: every split joined again, no
    // circle on. Apply carries it out.
    function chooseWay(one) {
        split = []
        beforeSplit = ({})
        chosen = ({})
        wholeSide = ""
        way = one
    }
    function splitRow(index) {
        if (split.indexOf(index) >= 0)
            return
        const before = Object.assign({}, beforeSplit)
        before[index] = chosen[index]
        beforeSplit = before
        const next = Object.assign({}, chosen)
        delete next[index]
        chosen = next
        split = split.concat([index])
        way = ""
    }
    function joinLast() {
        if (split.length === 0)
            return
        const index = split[split.length - 1]
        split = split.slice(0, -1)
        if (beforeSplit[index] !== undefined) {
            const next = Object.assign({}, chosen)
            next[index] = beforeSplit[index]
            chosen = next
        }
    }
    function clearChoices() {
        chosen = ({})
        split = []
        beforeSplit = ({})
        way = ""
        wholeSide = ""
    }
    function showItem(id) {
        itemId = id
        conflict = VaultController.conflictOf(id)
        clearChoices()
        reveal = false
        openedSize = Qt.size(0, 0)
        open()
        fitting = true
        fitWidth()
        Qt.callLater(fitHeight)
    }
    // The rows have their final widths once a frame was drawn: only then is
    // their height known.
    property bool fitting: false
    onFrameSwapped: {
        if (!fitting)
            return
        fitting = false
        fitHeight()
        Qt.callLater(function() { conflictWindow.openedSize = Qt.size(conflictWindow.width, conflictWindow.height) })
    }
    // As wide and tall as the person made it last, within the screen; until
    // then as tall as its first 11 fields.
    function given() {
        const parts = VaultController.windowValue("conflict/given").split(",")
        // Given at the UI scaling of that time, the third part (D-070).
        const by = parts.length === 3 ? Appearance.factor / parseFloat(parts[2]) : 1
        const size = parts.length >= 2 ? Qt.size(parseInt(parts[0]) * by, parseInt(parts[1]) * by) : Qt.size(0, 0)
        return size.width > 0 && size.height > 0 ? size : null
    }
    function fitWidth() {
        const room = Appearance.screenRoom(conflictWindow)
        const kept = given()
        const wanted = kept ? kept.width : 720 * Appearance.factor
        width = Math.max(minimumWidth, Math.min(wanted, room.width))
    }
    function fitHeight() {
        choices.ensurePolished()
        const room = Appearance.screenRoom(conflictWindow).height - Appearance.frameOf(conflictWindow).height
        const kept = given()
        const wanted = kept ? kept.height : minimumHeight - fields.smallest + Math.max(fields.smallest, heightOf(openRows))
        height = Math.max(Math.min(minimumHeight, room), Math.min(wanted, room))
    }
    // The table's height down to the end of this many fields.
    function heightOf(count) {
        const last = Math.min(count, allRows.length) - 1
        let end = choices.implicitHeight
        if (last >= 0 && last < allRows.length - 1) {
            end = 0
            for (const column of [names, lefts, rights]) {
                const cell = column.itemAt(last)
                if (cell)
                    end = Math.max(end, cell.y + cell.height)
            }
        }
        return Math.ceil(end)
    }
    function apply() {
        if (!ready)
            return
        if (way === "retry") {
            VaultController.retryConflicts([itemId])
        } else if (way !== "") {
            VaultController.settleConflicts([itemId], way)
        } else if (rows.length === 0) {
            VaultController.settleConflicts([itemId], wholeSide)
        } else {
            const choices = []
            for (let i = 0; i < rows.length; ++i)
                choices[rows[i].index] = split.indexOf(rows[i].index) >= 0 ? "both" : chosen[rows[i].index]
            for (let j = 0; j < choices.length; ++j) {
                if (choices[j] === undefined)
                    choices[j] = ""
            }
            VaultController.settleConflict(itemId, choices)
        }
        accept()
    }
    function shown_(text, hidden) {
        return hidden && !reveal ? "••••••••" : text
    }

    title: qsTr("Sync conflict: %1").arg(conflict.name || "")
    standardButtons: Dialog.NoButton
    defaultItem: applyButton
    minimumWidth: Math.max(Math.round(560 * Appearance.factor), contentLayout.implicitWidth + 2 * edge)

    onVisibleChanged: {
        if (!visible && openedSize.width > 0 && (width !== openedSize.width || height !== openedSize.height))
            VaultController.keepWindowValue("conflict/given", width + "," + height + "," + Appearance.factor)
    }

    leftButtons: [
        Button {
            objectName: "conflictRetryButton"
            text: qsTr("Retry sync")
            enabled: conflictWindow.conflict.canRetry === true
            onClicked: conflictWindow.chooseWay("retry")
        },
        Button {
            objectName: "conflictKeepHereButton"
            text: qsTr("Keep here only")
            enabled: conflictWindow.conflict.canKeepHere === true
            onClicked: conflictWindow.chooseWay("keep")
        },
        Button {
            objectName: "conflictKeepBothButton"
            text: qsTr("Keep both")
            enabled: conflictWindow.conflict.canKeepBoth === true
            icon.source: "qrc:/icons/keep-both.svg"
            icon.width: Math.round(16 * Appearance.factor)
            icon.height: Math.round(16 * Appearance.factor)
            icon.color: enabled ? conflictWindow.palette.buttonText : conflictWindow.palette.disabled.buttonText
            onClicked: conflictWindow.chooseWay("both")
        },
        // Apply and Cancel at the right.
        Item { Layout.fillWidth: true }
    ]
    rightButtons: [
        Button {
            id: applyButton
            objectName: "conflictApplyButton"
            text: qsTr("Apply")
            enabled: conflictWindow.ready
            highlighted: enabled
            onClicked: conflictWindow.apply()
        },
        Button {
            objectName: "conflictCancelButton"
            text: qsTr("Cancel")
            icon.source: "qrc:/icons/cancel.svg"
            icon.width: Math.round(16 * Appearance.factor)
            icon.height: Math.round(16 * Appearance.factor)
            icon.color: conflictWindow.palette.buttonText
            onClicked: conflictWindow.reject()
        }
    ]

    Shortcut {
        sequences: [StandardKey.Undo]
        enabled: Appearance.focusWindow === conflictWindow
        onActivated: conflictWindow.joinLast()
    }

    // A settled or vanished conflict (a sync, a lock) closes the window; one
    // that changed meanwhile is shown anew, with nothing chosen.
    Connections {
        target: VaultController
        function onRevisionChanged() {
            if (!conflictWindow.visible)
                return
            const now = VaultController.conflictOf(conflictWindow.itemId)
            if (now.itemId === undefined) {
                conflictWindow.reject()
            } else if (JSON.stringify(now) !== JSON.stringify(conflictWindow.conflict)) {
                conflictWindow.conflict = now
                conflictWindow.clearChoices()
            }
        }
    }

    // A choice with its circle in the second accent; its text whole.
    component Choice: RadioButton {
        id: choice
        autoExclusive: false
        // Shown as the window's choice only: a click never ticks it itself.
        checkable: false
        padding: 0
        spacing: conflictWindow.circleGap
        indicator: Rectangle {
            x: choice.leftPadding
            // In the middle of the text's first line.
            y: choice.topPadding + Math.round((lineHeight.height * Appearance.factor - height) / 2)
            implicitWidth: conflictWindow.circleSize
            implicitHeight: implicitWidth
            radius: width / 2
            color: "transparent"
            border.width: conflictWindow.line
            border.color: conflictWindow.accent
            Rectangle {
                anchors.centerIn: parent
                width: Math.round(parent.width * 0.5)
                height: width
                radius: width / 2
                color: conflictWindow.accent
                visible: choice.checked
            }
            Rectangle {
                anchors.fill: parent
                anchors.margins: -conflictWindow.line
                radius: width / 2
                color: "transparent"
                border.width: choice.visualFocus ? 1 : 0
                border.color: conflictWindow.palette.highlight
            }
        }
        contentItem: Label {
            leftPadding: conflictWindow.circleSize + choice.spacing
            text: choice.text
            font: choice.font
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }
        FontMetrics { id: lineHeight; font: choice.font }
    }

    // One side of one field: outlined with its choice, plain, or both values.
    component Cell: Item {
        id: cell
        required property var row
        required property string side
        readonly property bool isSplit: conflictWindow.split.indexOf(row.index) >= 0
        readonly property bool choosing: row.differs && !isSplit
        implicitWidth: 120 * Appearance.factor
        implicitHeight: choosing ? outline.height : texts.height
        Rectangle {
            id: outline
            visible: cell.choosing
            width: cell.width
            height: option.implicitHeight + 2 * (conflictWindow.line + conflictWindow.innerTop)
            color: "transparent"
            radius: Math.round(4 * Appearance.factor)
            border.width: conflictWindow.line
            border.color: conflictWindow.accent
            Choice {
                id: option
                objectName: (cell.side === "left" ? "conflictLeft_" : "conflictRight_") + cell.row.index
                x: conflictWindow.line + conflictWindow.innerSide
                y: conflictWindow.line + conflictWindow.innerTop
                width: parent.width - 2 * x
                text: conflictWindow.shown_(cell.side === "left" ? cell.row.left : cell.row.right,
                                            cell.side === "left" ? cell.row.leftHidden : cell.row.rightHidden)
                checked: conflictWindow.way === "" && conflictWindow.chosen[cell.row.index] === cell.side
                onClicked: conflictWindow.choose(cell.row.index, cell.side)
            }
        }
        Column {
            id: texts
            visible: !cell.choosing
            width: cell.width
            spacing: choices.rowSpacing
            Repeater {
                model: cell.isSplit
                       ? [{ text: cell.row.first, hidden: cell.row.leftHidden },
                          { text: cell.row.second, hidden: cell.row.rightHidden }]
                       : [{ text: cell.side === "left" ? cell.row.left : cell.row.right,
                            hidden: cell.side === "left" ? cell.row.leftHidden : cell.row.rightHidden }]
                delegate: Label {
                    required property var modelData
                    width: texts.width
                    leftPadding: conflictWindow.textLeft
                    rightPadding: conflictWindow.line + conflictWindow.innerSide
                    topPadding: conflictWindow.line + conflictWindow.innerTop
                    bottomPadding: topPadding
                    text: conflictWindow.shown_(modelData.text, modelData.hidden)
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    ExactColumn {
        id: contentLayout
        gap: 8 * Appearance.factor
        Layout.fillWidth: true
        Layout.fillHeight: true

        GridLayout {
            columns: 2
            columnSpacing: 12 * Appearance.factor
            rowSpacing: 4 * Appearance.factor
            Layout.fillWidth: true
            Label { text: qsTr("What") }
            Label { text: conflictWindow.conflict.what || ""; textFormat: Text.PlainText; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: qsTr("Where") }
            Label { text: conflictWindow.conflict.where || ""; textFormat: Text.PlainText; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: qsTr("Why") }
            Label {
                objectName: "conflictWhy"
                text: conflictWindow.kind === "deletedThere" ? qsTr("Deleted on the other device, changed here")
                      : conflictWindow.kind === "deletedHere" ? qsTr("Deleted here, changed on the other device")
                      : conflictWindow.kind === "imported" ? qsTr("Differs from an import")
                      : conflictWindow.conflict.source === "mixed"
                        ? qsTr("Changed on two devices, and differs from an import")
                        : qsTr("Changed on two devices")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label { text: qsTr("Found") }
            Label { objectName: "conflictFound"; text: conflictWindow.conflict.found || "" }
        }

        TickBox {
            objectName: "conflictRevealBox"
            visible: conflictWindow.anyHidden
            text: qsTr("Show hidden values")
            checked: conflictWindow.reveal
            onToggled: conflictWindow.reveal = checked
        }

        ScrollView {
            id: fields
            objectName: "conflictFields"
            // The header and about three rows.
            readonly property real smallest: 130 * Appearance.factor
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: smallest
            contentWidth: availableWidth
            contentHeight: choices.implicitHeight
            clip: true
            // The normal scroll bar (D-049), in room of its own beside the
            // rows, so no outline lies under it.
            rightPadding: fieldsBar.width + 6 * Appearance.factor
            ScrollBar.vertical: ScrollBar {
                id: fieldsBar
                objectName: "conflictScrollBar"
                parent: fields
                x: fields.mirrored ? 0 : fields.width - width
                y: fields.topPadding
                height: fields.availableHeight
                orientation: Qt.Vertical
            }

            GridLayout {
                id: choices
                width: fields.availableWidth
                columns: 4
                columnSpacing: 10 * Appearance.factor
                rowSpacing: 4 * Appearance.factor

                Label {
                    text: qsTr("Field")
                    font.bold: true
                    Layout.row: 0
                    Layout.column: 0
                }
                // The header's choices stand where the rows' do.
                Choice {
                    objectName: "conflictAllLeft"
                    text: conflictWindow.conflict.leftName || ""
                    font.bold: true
                    checked: conflictWindow.allAre("left")
                    onClicked: conflictWindow.chooseAll("left")
                    Layout.row: 0
                    Layout.column: 1
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.leftMargin: conflictWindow.line + conflictWindow.innerSide
                }
                Choice {
                    objectName: "conflictAllRight"
                    text: conflictWindow.conflict.rightName || ""
                    font.bold: true
                    checked: conflictWindow.allAre("right")
                    onClicked: conflictWindow.chooseAll("right")
                    Layout.row: 0
                    Layout.column: 2
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.leftMargin: conflictWindow.line + conflictWindow.innerSide
                }

                Repeater {
                    id: names
                    model: conflictWindow.allRows
                    delegate: Column {
                        id: nameCell
                        required property var modelData
                        required property int index
                        Layout.row: index + 1
                        Layout.column: 0
                        Layout.alignment: Qt.AlignTop
                        Layout.maximumWidth: 200 * Appearance.factor
                        spacing: choices.rowSpacing
                        Repeater {
                            model: conflictWindow.split.indexOf(nameCell.modelData.index) >= 0 ? 2 : 1
                            delegate: Label {
                                width: Math.min(implicitWidth, 200 * Appearance.factor)
                                topPadding: conflictWindow.line + conflictWindow.innerTop
                                bottomPadding: topPadding
                                text: nameCell.modelData.label
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
                Repeater {
                    id: lefts
                    model: conflictWindow.allRows
                    delegate: Cell {
                        required property var modelData
                        required property int index
                        row: modelData
                        side: "left"
                        Layout.row: index + 1
                        Layout.column: 1
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignTop
                    }
                }
                Repeater {
                    id: rights
                    model: conflictWindow.allRows
                    delegate: Cell {
                        required property var modelData
                        required property int index
                        row: modelData
                        side: "right"
                        Layout.row: index + 1
                        Layout.column: 2
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignTop
                    }
                }
                Repeater {
                    model: conflictWindow.allRows
                    delegate: ToolButton {
                        required property var modelData
                        required property int index
                        objectName: "conflictSplit_" + modelData.index
                        visible: modelData.canSplit
                        enabled: conflictWindow.split.indexOf(modelData.index) < 0
                        Layout.row: index + 1
                        Layout.column: 3
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredHeight: lineHeightOfRow.height * Appearance.factor + 2 * (conflictWindow.line + conflictWindow.innerTop)
                        Layout.preferredWidth: implicitWidth
                        padding: 0
                        horizontalPadding: 1 + 5 * Appearance.factor
                        icon.source: "qrc:/icons/split.svg"
                        icon.width: Math.round(20 * Appearance.factor)
                        icon.height: Math.round(20 * Appearance.factor)
                        icon.color: enabled ? conflictWindow.palette.buttonText : conflictWindow.palette.disabled.buttonText
                        Accessible.name: qsTr("Keep both")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Keep both")
                        onClicked: conflictWindow.splitRow(modelData.index)
                    }
                }
            }
        }
    }
    FontMetrics { id: lineHeightOfRow; font: conflictWindow.font }
}
