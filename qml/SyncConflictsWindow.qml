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
import QtQuick.Controls.impl
import QtQuick.Layouts

// --- Version ------------------------------------------------------------
// SyncConflictsWindow.qml 0.6.103

// --- Version history ----------------------------------------------------
// v0.6.103: One standard dialog for a master password, a PIN or a finger (D-073)
// v0.6.94: The main window keeps its size, the others scale along (D-070)
// v0.6.92: Exact sizes (D-069)
// v0.5.82: Every key of a list, Ctrl+A, ranges (D-062)
// v0.5.81: The list as Chrono Mail's: a table whose columns are dragged, line icons, Retry sync, Keep here only, every button greyed and never hidden (D-061)
// v0.5.78: The buttons "Use Vault" and "Use Import", or "Use This device" and "Use Other device"; the row tick boxes as clear as Check all (D-061)
// v0.5.77: A conflict from an import has its own mark (D-060)
// v0.5.76: Initial release (D-060)

// Sync conflicts (Tools > Sync conflicts…), as Chrono Mail's (D-060, D-061):
// one row per item with a conflict, with What, Name, Fields, Where and Why;
// the ticked rows settled whole, or one by one with a double click. Nothing
// asks during a sync: a conflict waits here until the person settles it.
DialogWindow {
    id: conflictsWindow
    objectName: "syncConflictsWindow"
    memoryName: "syncConflicts"
    modality: Qt.NonModal

    property var rows: []
    // The ticked rows, by item id.
    property var ticked: ({})
    readonly property var tickedRows: rows.filter(function(row) { return ticked[row.itemId] === true })
    readonly property int tickedCount: tickedRows.length
    function everyTicked(can) {
        return tickedCount > 0 && tickedRows.every(function(row) { return row[can] === true })
    }
    // The sides' names of the ticked rows (D-061): an import's, a sync's, or
    // both when the ticked rows hold both.
    readonly property var sides: {
        let imported = false
        let synced = false
        for (let i = 0; i < tickedRows.length; ++i) {
            if (tickedRows[i].source !== "both")
                imported = true
            if (tickedRows[i].source !== "imported")
                synced = true
        }
        return VaultController.sideNames(imported && synced ? "mixed" : imported ? "imported" : "both")
    }
    readonly property var whyWords: ({
        "both": qsTr("Changed on two devices"),
        "deletedThere": qsTr("Deleted on the other device, changed here"),
        "deletedHere": qsTr("Deleted here, changed on the other device"),
        "imported": qsTr("Differs from an import")
    })
    // Retry sync's answer, under the buttons.
    property string said: ""
    property bool saidTrouble: false

    // The columns: What, Name, Fields and Where together always the free
    // width, each its share of it; What its widest text until it is dragged;
    // none narrower than its header. The tick box and Why keep their width.
    readonly property var columnNames: [qsTr("What"), qsTr("Name"), qsTr("Fields"), qsTr("Where")]
    property var shares: [0.12, 0.38, 0.2, 0.3]
    property bool whatDragged: false
    property var columnWidths: [80, 200, 120, 160]
    readonly property real cellPad: 6 * Appearance.factor
    readonly property real tickWidth: 34 * Appearance.factor
    readonly property real whyWidth: Math.ceil(metrics.advanceWidth(qsTr("Why")) * Appearance.factor) + 28 * Appearance.factor
    readonly property real barRoom: list.contentHeight > list.height ? 16 * Appearance.factor : 0
    readonly property int freeWidth: Math.max(0, table.width - tickWidth - whyWidth - barRoom)
    onFreeWidthChanged: fitColumns()
    function smallestOf(column) {
        return Math.ceil(metrics.advanceWidth(columnNames[column]) * Appearance.factor) + 2 * cellPad + 4
    }
    function fitColumns() {
        if (freeWidth <= 0)
            return
        const smallest = [0, 1, 2, 3].map(smallestOf)
        const widths = [0, 0, 0, 0]
        let rest = [0, 1, 2, 3]
        let room = freeWidth
        if (!whatDragged) {
            let what = smallest[0]
            for (let i = 0; i < rows.length; ++i)
                what = Math.max(what, Math.ceil(metrics.advanceWidth(rows[i].what || "") * Appearance.factor) + 2 * cellPad + 2)
            widths[0] = Math.max(smallest[0], Math.min(what, freeWidth - smallest[1] - smallest[2] - smallest[3]))
            rest = [1, 2, 3]
            room -= widths[0]
        }
        let total = 0
        for (const column of rest)
            total += shares[column]
        for (const column of rest)
            widths[column] = Math.max(smallest[column], Math.floor(room * shares[column] / total))
        let over = widths[0] + widths[1] + widths[2] + widths[3] - freeWidth
        let widest = rest[0]
        for (const column of rest) {
            if (widths[column] > widths[widest])
                widest = column
        }
        widths[widest] = Math.max(smallest[widest], widths[widest] - over)
        columnWidths = widths
    }
    // A border dragged: its column grows by what the one at its right gives.
    function dragBorder(column, by) {
        if (column > 2)
            return
        const widths = columnWidths.slice()
        const grown = Math.max(smallestOf(column) - widths[column],
                               Math.min(by, widths[column + 1] - smallestOf(column + 1)))
        widths[column] += grown
        widths[column + 1] -= grown
        if (column === 0)
            whatDragged = true
        columnWidths = widths
        const total = widths[0] + widths[1] + widths[2] + widths[3]
        shares = widths.map(function(width) { return width / total })
    }
    function keepColumns() {
        VaultController.keepWindowValue("syncConflicts/columns",
                                        shares.map(function(share) { return share.toFixed(4) }).join(",")
                                        + "," + (whatDragged ? "1" : "0"))
    }
    function readColumns() {
        const parts = VaultController.windowValue("syncConflicts/columns").split(",")
        if (parts.length !== 5)
            return
        const read = parts.slice(0, 4).map(parseFloat)
        const total = read[0] + read[1] + read[2] + read[3]
        if (read.every(function(share) { return share > 0 && share < 1 }) && Math.abs(total - 1) < 0.01) {
            shares = read
            whatDragged = parts[4] === "1"
        }
    }

    function refresh() {
        rows = VaultController.conflicts()
        const kept = {}
        for (let i = 0; i < rows.length; ++i) {
            if (ticked[rows[i].itemId])
                kept[rows[i].itemId] = true
        }
        ticked = kept
        fitColumns()
    }
    function tick(id, on) {
        const next = Object.assign({}, ticked)
        if (on)
            next[id] = true
        else
            delete next[id]
        ticked = next
    }
    function setAll(on) {
        const next = {}
        for (let i = 0; i < rows.length; ++i) {
            if (on === null ? !ticked[rows[i].itemId] : on)
                next[rows[i].itemId] = true
        }
        ticked = next
    }
    function tickedIds() {
        return tickedRows.map(function(row) { return row.itemId })
    }
    function settle(side) {
        said = ""
        VaultController.settleConflicts(tickedIds(), side)
    }
    function showItem(id) {
        refresh()
        open()
        conflictWindow.showItem(id)
    }

    title: qsTr("Sync conflicts")
    standardButtons: Dialog.Close
    firstWidth: 1000 * Appearance.factor
    firstHeight: 520 * Appearance.factor

    Component.onCompleted: readColumns()
    onShown: {
        said = ""
        refresh()
        list.forceActiveFocus()
        if (list.currentIndex < 0 && rows.length > 0)
            list.currentIndex = 0
    }
    onVisibleChanged: {
        if (!visible)
            keepColumns()
    }
    Connections {
        target: VaultController
        function onRevisionChanged() { conflictsWindow.refresh() }
        function onStateChanged() {
            if (VaultController.state !== VaultController.Unlocked)
                conflictsWindow.reject()
        }
        function onRetried(text, trouble) {
            conflictsWindow.said = text
            conflictsWindow.saidTrouble = trouble
        }
        function onRetryPasswordNeeded(source, vaultName, itemIds, problem) {
            retryPassword.askNext(source, vaultName, itemIds, problem)
        }
    }

    FontMetrics { id: metrics; font: conflictsWindow.font }

    component Why: IconLabel {
        property string kind: "both"
        display: AbstractButton.IconOnly
        icon.source: "qrc:/icons/conflict-" + (kind === "deletedThere" ? "deleted-there"
                                               : kind === "deletedHere" ? "deleted-here"
                                               : kind === "imported" ? "import" : "both") + ".svg"
        icon.width: Math.round(18 * Appearance.factor)
        icon.height: Math.round(18 * Appearance.factor)
        icon.color: conflictsWindow.palette.text
    }
    component Line: Rectangle {
        width: 1
        color: conflictsWindow.palette.mid
    }
    // A cell's text; cut off, it shows whole on hovering.
    component CellText: Label {
        id: cellText
        textFormat: Text.PlainText
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        leftPadding: conflictsWindow.cellPad
        rightPadding: conflictsWindow.cellPad
        HoverHandler { id: over }
        ToolTip.visible: over.hovered && truncated
        ToolTip.text: text
        ToolTip.delay: 500
    }

    ExactColumn {
        gap: 8 * Appearance.factor
        Layout.fillWidth: true
        Layout.fillHeight: true

        TickBox {
            objectName: "conflictsCheckAll"
            text: qsTr("Check all")
            enabled: conflictsWindow.rows.length > 0
            // Shown as the ticks are only: a click never ticks it itself.
            checkable: false
            checked: conflictsWindow.rows.length > 0 && conflictsWindow.tickedCount === conflictsWindow.rows.length
            onClicked: conflictsWindow.setAll(!checked)
        }

        Rectangle {
            id: table
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 160 * Appearance.factor
            color: conflictsWindow.palette.base

            // The header bar.
            Rectangle {
                id: header
                width: parent.width
                height: metrics.height * Appearance.factor + 8 * Appearance.factor
                color: conflictsWindow.palette.button
                Row {
                    height: parent.height
                    Item {
                        width: conflictsWindow.tickWidth
                        height: parent.height
                        Line { anchors.right: parent.right; height: parent.height }
                    }
                    Repeater {
                        model: 4
                        delegate: Item {
                            id: headerCell
                            required property int index
                            objectName: "conflictsHeader_" + index
                            width: conflictsWindow.columnWidths[index]
                            height: header.height
                            Label {
                                anchors.fill: parent
                                leftPadding: conflictsWindow.cellPad
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                text: conflictsWindow.columnNames[headerCell.index]
                            }
                            Line { anchors.right: parent.right; height: parent.height }
                            MouseArea {
                                property real from: 0
                                anchors.right: parent.right
                                anchors.rightMargin: -4 * Appearance.factor
                                width: 8 * Appearance.factor
                                height: parent.height
                                enabled: headerCell.index < 3
                                z: 1
                                cursorShape: enabled ? Qt.SplitHCursor : Qt.ArrowCursor
                                preventStealing: true
                                onPressed: function(mouse) { from = mapToItem(header, mouse.x, 0).x }
                                onPositionChanged: function(mouse) {
                                    const at = mapToItem(header, mouse.x, 0).x
                                    const before = conflictsWindow.columnWidths[headerCell.index]
                                    conflictsWindow.dragBorder(headerCell.index, Math.round(at - from))
                                    from += conflictsWindow.columnWidths[headerCell.index] - before
                                }
                            }
                        }
                    }
                    Label {
                        width: conflictsWindow.whyWidth
                        height: parent.height
                        text: qsTr("Why")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
            ListView {
                id: list
                objectName: "conflictsList"
                anchors.top: header.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: conflictsWindow.rows
                currentIndex: -1
                focus: true
                activeFocusOnTab: true
                keyNavigationEnabled: false
                highlightMoveDuration: 0
                ScrollBar.vertical: ScrollBar {}
                // Where a range of ticks begins.
                property int anchorRow: 0
                function tickRange(row) {
                    const from = Math.max(0, Math.min(anchorRow, count - 1))
                    const next = Object.assign({}, conflictsWindow.ticked)
                    for (let one = Math.min(from, row); one <= Math.max(from, row); ++one)
                        next[conflictsWindow.rows[one].itemId] = true
                    conflictsWindow.ticked = next
                }
                Keys.onPressed: function(event) {
                    const modifiers = event.modifiers & ~Qt.KeypadModifier
                    if (modifiers === Qt.ControlModifier && event.key === Qt.Key_A) {
                        event.accepted = true
                        conflictsWindow.setAll(true)
                        return
                    }
                    const shift = modifiers === Qt.ShiftModifier
                    if ((modifiers !== Qt.NoModifier && !shift) || count === 0)
                        return
                    const page = Math.max(1, Math.floor(height / Math.max(1, currentItem ? currentItem.height : 24)) - 1)
                    const here = conflictsWindow.rows[currentIndex]
                    let to = currentIndex
                    switch (event.key) {
                    case Qt.Key_Up: to = currentIndex - 1; break
                    case Qt.Key_Down: to = currentIndex + 1; break
                    case Qt.Key_PageUp: to = currentIndex - page; break
                    case Qt.Key_PageDown: to = currentIndex + page; break
                    case Qt.Key_Home: to = 0; break
                    case Qt.Key_End: to = count - 1; break
                    case Qt.Key_Space:
                        event.accepted = true
                        if (here)
                            conflictsWindow.tick(here.itemId, !conflictsWindow.ticked[here.itemId])
                        return
                    case Qt.Key_Return:
                    case Qt.Key_Enter:
                        event.accepted = true
                        if (here)
                            conflictWindow.showItem(here.itemId)
                        return
                    default: return
                    }
                    event.accepted = true
                    to = Math.max(0, Math.min(count - 1, to))
                    if (shift) {
                        tickRange(to)
                    } else {
                        anchorRow = to
                    }
                    currentIndex = to
                    positionViewAtIndex(to, ListView.Contain)
                }
                delegate: Rectangle {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property bool selected: ListView.isCurrentItem
                    readonly property color ink: selected ? conflictsWindow.palette.highlightedText : conflictsWindow.palette.text
                    objectName: "conflictRow_" + index
                    width: list.width
                    height: metrics.height * Appearance.factor + 8 * Appearance.factor
                    color: selected ? conflictsWindow.palette.highlight : "transparent"
                    signal doubleClicked()
                    onDoubleClicked: conflictWindow.showItem(modelData.itemId)
                    MouseArea {
                        anchors.fill: parent
                        onClicked: function(mouse) {
                            list.forceActiveFocus()
                            list.currentIndex = row.index
                            if (mouse.modifiers & Qt.ShiftModifier) {
                                list.tickRange(row.index)
                                return
                            }
                            list.anchorRow = row.index
                            if (mouse.modifiers & Qt.ControlModifier)
                                conflictsWindow.tick(row.modelData.itemId, !conflictsWindow.ticked[row.modelData.itemId])
                        }
                        onDoubleClicked: row.doubleClicked()
                    }
                    Row {
                        height: parent.height
                        Item {
                            width: conflictsWindow.tickWidth
                            height: parent.height
                            TickBox {
                                objectName: "conflictTick_" + row.index
                                anchors.centerIn: parent
                                padding: 0
                                focusPolicy: Qt.NoFocus
                                checkable: false
                                checked: conflictsWindow.ticked[row.modelData.itemId] === true
                                onClicked: conflictsWindow.tick(row.modelData.itemId, !checked)
                            }
                        }
                        CellText { width: conflictsWindow.columnWidths[0]; height: row.height; color: row.ink; text: row.modelData.what }
                        CellText { width: conflictsWindow.columnWidths[1]; height: row.height; color: row.ink; text: row.modelData.name }
                        CellText { width: conflictsWindow.columnWidths[2]; height: row.height; color: row.ink; text: row.modelData.fields }
                        CellText { width: conflictsWindow.columnWidths[3]; height: row.height; color: row.ink; text: row.modelData.where }
                        Item {
                            width: conflictsWindow.whyWidth
                            height: parent.height
                            Why {
                                anchors.centerIn: parent
                                kind: row.modelData.kind
                                icon.color: row.ink
                            }
                            HoverHandler { id: overWhy }
                            ToolTip.visible: overWhy.hovered
                            ToolTip.text: conflictsWindow.whyWords[row.modelData.kind] || ""
                            ToolTip.delay: 500
                        }
                    }
                }
            }
            Label {
                visible: conflictsWindow.rows.length === 0
                anchors.top: header.bottom
                anchors.left: parent.left
                anchors.margins: 8 * Appearance.factor
                text: qsTr("No conflicts to settle.")
                opacity: 0.7
            }
        }

        // The ticked rows, settled whole. Greyed, never hidden.
        ExactRow {
            gap: 6 * Appearance.factor
            Button {
                objectName: "useLeftButton"
                text: qsTr("Use %1").arg(conflictsWindow.sides.left)
                enabled: conflictsWindow.tickedCount > 0
                onClicked: conflictsWindow.settle("left")
            }
            Button {
                objectName: "useRightButton"
                text: qsTr("Use %1").arg(conflictsWindow.sides.right)
                enabled: conflictsWindow.tickedCount > 0
                onClicked: conflictsWindow.settle("right")
            }
            Button {
                objectName: "retryButton"
                text: qsTr("Retry sync")
                enabled: conflictsWindow.everyTicked("canRetry")
                onClicked: {
                    conflictsWindow.said = ""
                    VaultController.retryConflicts(conflictsWindow.tickedIds())
                }
            }
            Button {
                objectName: "keepHereButton"
                text: qsTr("Keep here only")
                enabled: conflictsWindow.everyTicked("canKeepHere")
                onClicked: conflictsWindow.settle("keep")
            }
            Button {
                objectName: "keepBothButton"
                text: qsTr("Keep both")
                enabled: conflictsWindow.everyTicked("canKeepBoth")
                onClicked: conflictsWindow.settle("both")
            }
            Button {
                objectName: "invertSelectionButton"
                text: qsTr("Invert selection")
                enabled: conflictsWindow.rows.length > 0
                onClicked: conflictsWindow.setAll(null)
            }
        }
        Label {
            objectName: "conflictsSaid"
            visible: text !== ""
            text: conflictsWindow.said
            color: conflictsWindow.saidTrouble ? "#e53e3e" : conflictsWindow.palette.windowText
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        // What the Why icons mean, in the same words as their hover texts.
        Repeater {
            model: ["both", "deletedThere", "deletedHere", "imported"]
            delegate: ExactRow {
                id: legend
                required property string modelData
                gap: 6 * Appearance.factor
                Why { kind: legend.modelData; icon.color: conflictsWindow.palette.windowText }
                Label { text: conflictsWindow.whyWords[legend.modelData] }
            }
        }
    }

    ConflictWindow {
        id: conflictWindow
        transientParent: conflictsWindow
    }

    // Retry sync from a vault whose key is not at hand: its master password.
    AskWindow {
        id: retryPassword
        objectName: "retryPasswordWindow"
        transientParent: conflictsWindow
        title: qsTr("Retry sync")
        // Those still to be asked; the first is the one shown.
        property var waiting: []
        function askNext(source, vaultName, itemIds, problem) {
            waiting = waiting.concat([{ source: source, name: vaultName, ids: itemIds, problem: problem }])
            if (!visible)
                showFirst()
        }
        function showFirst() {
            if (waiting.length === 0)
                return
            picture = String(waiting[0].source).indexOf("vault:") === 0 ? "qrc:/icons/storage-device.svg"
                                                                        : "qrc:/icons/storage-webdav.svg"
            ask(waiting[0].name, waiting[0].problem)
        }
        Connections {
            target: VaultController
            function onLocking() {
                retryPassword.waiting = []
                if (retryPassword.visible)
                    retryPassword.reject()
            }
        }
        onEntered: function(text) {
            const one = waiting[0]
            waiting = waiting.slice(1)
            accept()
            if (one)
                VaultController.retryFromVault(one.source, one.ids, text)
            Qt.callLater(showFirst)
        }
        onRejected: {
            waiting = waiting.slice(1)
            Qt.callLater(showFirst)
        }
    }
}
