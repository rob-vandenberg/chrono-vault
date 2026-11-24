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
// SyncConflictsWindow.qml 0.5.76

// --- Version history ----------------------------------------------------
// v0.5.76: Initial release (D-060)

// Sync conflicts (Tools > Sync conflicts…), after Chrono Mail's (D-060): one
// row per item with a conflict, with What, Name, Fields, Where and Why; the
// ticked rows settled whole with Use value now, Use other value or Keep both,
// or one by one with a double click. Nothing asks during a sync: a conflict
// waits here until the person settles it.
DialogWindow {
    id: conflictsWindow
    objectName: "syncConflictsWindow"
    memoryName: "syncConflicts"
    modality: Qt.NonModal

    property var rows: []
    // The ticked rows, by item id.
    property var ticked: ({})
    readonly property int tickedCount: {
        let count = 0
        for (let i = 0; i < rows.length; ++i) {
            if (ticked[rows[i].itemId])
                ++count
        }
        return count
    }
    readonly property bool anyDeleted: {
        for (let i = 0; i < rows.length; ++i) {
            if (ticked[rows[i].itemId] && rows[i].kind === "deleted")
                return true
        }
        return false
    }
    readonly property int whatWidth: Math.round(110 * Appearance.factor)
    readonly property int whyWidth: Math.round(44 * Appearance.factor)
    readonly property int tickWidth: Math.round(32 * Appearance.factor)

    function refresh() {
        rows = VaultController.conflicts()
        const kept = {}
        for (let i = 0; i < rows.length; ++i) {
            if (ticked[rows[i].itemId])
                kept[rows[i].itemId] = true
        }
        ticked = kept
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
    function settle(side) {
        const ids = []
        for (let i = 0; i < rows.length; ++i) {
            if (ticked[rows[i].itemId])
                ids.push(rows[i].itemId)
        }
        VaultController.settleConflicts(ids, side)
    }
    function showItem(id) {
        refresh()
        open()
        conflictWindow.showItem(id)
    }

    title: qsTr("Sync conflicts")
    standardButtons: Dialog.Close
    width: Math.round(820 * Appearance.factor)
    height: Math.round(460 * Appearance.factor)

    onShown: refresh()
    Connections {
        target: VaultController
        function onRevisionChanged() { conflictsWindow.refresh() }
        function onStateChanged() {
            if (VaultController.state !== VaultController.Unlocked)
                conflictsWindow.reject()
        }
    }

    component Why: Rectangle {
        property string kind: "both"
        implicitWidth: Math.round(20 * Appearance.factor)
        implicitHeight: implicitWidth
        radius: width / 2
        color: kind === "deleted" ? "#d32f2f" : "#ef6c00"
        Label {
            anchors.centerIn: parent
            text: parent.kind === "deleted" ? "−" : "≠"
            color: "white"
            font.bold: true
        }
    }

    ColumnLayout {
        spacing: Math.round(8 * Appearance.factor)
        Layout.fillWidth: true
        Layout.fillHeight: true

        CheckBox {
            objectName: "conflictsCheckAll"
            text: qsTr("Check all")
            enabled: conflictsWindow.rows.length > 0
            // Shown as the ticks are only: a click never ticks it itself.
            checkable: false
            checked: conflictsWindow.rows.length > 0 && conflictsWindow.tickedCount === conflictsWindow.rows.length
            onClicked: conflictsWindow.setAll(!checked)
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Math.round(160 * Appearance.factor)
            padding: 0
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                // The header.
                RowLayout {
                    spacing: Math.round(6 * Appearance.factor)
                    Layout.fillWidth: true
                    Layout.margins: Math.round(4 * Appearance.factor)
                    Item { Layout.preferredWidth: conflictsWindow.tickWidth }
                    Label { text: qsTr("What"); font.bold: true; Layout.preferredWidth: conflictsWindow.whatWidth }
                    Label { text: qsTr("Name"); font.bold: true; Layout.fillWidth: true; Layout.preferredWidth: 3 }
                    Label { text: qsTr("Fields"); font.bold: true; Layout.fillWidth: true; Layout.preferredWidth: 2 }
                    Label { text: qsTr("Where"); font.bold: true; Layout.fillWidth: true; Layout.preferredWidth: 2 }
                    Label {
                        text: qsTr("Why")
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        Layout.preferredWidth: conflictsWindow.whyWidth
                    }
                }
                ListView {
                    id: list
                    objectName: "conflictsList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: conflictsWindow.rows
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        id: row
                        required property var modelData
                        required property int index
                        objectName: "conflictRow_" + index
                        width: list.width
                        highlighted: conflictsWindow.ticked[modelData.itemId] === true
                        onDoubleClicked: conflictWindow.showItem(modelData.itemId)
                        contentItem: RowLayout {
                            spacing: Math.round(6 * Appearance.factor)
                            CheckBox {
                                objectName: "conflictTick_" + row.index
                                checkable: false
                                checked: conflictsWindow.ticked[row.modelData.itemId] === true
                                onClicked: conflictsWindow.tick(row.modelData.itemId, !checked)
                                Layout.preferredWidth: conflictsWindow.tickWidth
                            }
                            Label {
                                text: row.modelData.what
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                Layout.preferredWidth: conflictsWindow.whatWidth
                            }
                            Label {
                                text: row.modelData.name
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                Layout.preferredWidth: 3
                            }
                            Label {
                                text: row.modelData.fields
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                Layout.preferredWidth: 2
                            }
                            Label {
                                text: row.modelData.where
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                Layout.preferredWidth: 2
                            }
                            Item {
                                Layout.preferredWidth: conflictsWindow.whyWidth
                                implicitHeight: whyMark.implicitHeight
                                Why {
                                    id: whyMark
                                    anchors.centerIn: parent
                                    kind: row.modelData.kind
                                }
                            }
                        }
                    }
                }
                Label {
                    visible: conflictsWindow.rows.length === 0
                    text: qsTr("No conflicts to settle.")
                    opacity: 0.7
                    Layout.margins: Math.round(8 * Appearance.factor)
                }
            }
        }

        // The ticked rows, settled whole.
        RowLayout {
            spacing: Math.round(6 * Appearance.factor)
            Button {
                objectName: "useValueNowButton"
                text: qsTr("Use value now")
                enabled: conflictsWindow.tickedCount > 0
                onClicked: conflictsWindow.settle("now")
            }
            Button {
                objectName: "useOtherValueButton"
                text: qsTr("Use other value")
                enabled: conflictsWindow.tickedCount > 0
                onClicked: conflictsWindow.settle("other")
            }
            Button {
                objectName: "keepBothButton"
                text: qsTr("Keep both")
                enabled: conflictsWindow.tickedCount > 0 && !conflictsWindow.anyDeleted
                onClicked: conflictsWindow.settle("both")
            }
            Button {
                objectName: "invertSelectionButton"
                text: qsTr("Invert selection")
                enabled: conflictsWindow.rows.length > 0
                onClicked: conflictsWindow.setAll(null)
            }
        }

        // What the Why marks mean.
        RowLayout {
            spacing: Math.round(6 * Appearance.factor)
            Why { kind: "both" }
            Label { text: qsTr("Changed on two sides") }
            Item { Layout.preferredWidth: Math.round(16 * Appearance.factor) }
            Why { kind: "deleted" }
            Label { text: qsTr("Deleted on one side and changed on the other (Use other value moves it to the Trash)") }
        }
    }

    ConflictWindow {
        id: conflictWindow
        transientParent: conflictsWindow
    }
}
