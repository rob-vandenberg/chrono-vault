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
// VaultOverview.qml 0.6.88

// --- Version history ----------------------------------------------------
// v0.6.88: Settings > Categories and Settings > Tags (D-066)
// v0.6.86: The audit above the counts (D-065)
// v0.5.83: Initial release (D-063)

// The right side while no item is focused: the vault's name and its counts,
// each a way to what it counts, under what the audit found.
ScrollView {
    id: overview
    signal filterChosen(string filter)
    signal categoriesRequested()
    signal tagsRequested()
    signal leakSettingRequested()
    readonly property var audit: VaultController.audit
    contentWidth: availableWidth

    component Count: AbstractButton {
        id: card
        property int count: 0
        // Words in place of the number, and a colour of its own.
        property string instead: ""
        property color numberColour: palette.text
        Layout.fillWidth: true
        Layout.preferredWidth: Math.round(160 * Appearance.factor)
        padding: Math.round(14 * Appearance.factor)
        hoverEnabled: true
        Accessible.name: text + " " + (instead !== "" ? instead : count)
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        readonly property color ink: palette.text
        background: Rectangle {
            radius: Math.round(6 * Appearance.factor)
            color: Qt.rgba(card.ink.r, card.ink.g, card.ink.b, card.down ? 0.14 : card.hovered ? 0.10 : 0.06)
            border.width: card.visualFocus ? Math.max(1, Math.round(2 * Appearance.factor)) : 0
            border.color: card.palette.highlight
        }
        contentItem: ColumnLayout {
            spacing: Math.round(2 * Appearance.factor)
            Label {
                text: card.text
                opacity: 0.7
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                text: card.instead !== "" ? card.instead : card.count
                color: card.numberColour
                elide: Text.ElideRight
                font.pointSize: Appearance.fontPointSize * 1.8
                Layout.fillWidth: true
            }
        }
    }
    // A list of the audit: red with something in it, green without.
    component AuditCard: Count {
        property string list: ""
        property string state: "done"
        readonly property bool known: overview.audit.ready && state === "done"
        objectName: "auditCard_" + list
        count: overview.audit[list]
        instead: count > 0 && overview.audit.ready ? ""
                 : state === "failed" ? qsTr("Not checked")
                 : state === "off" && !VaultController.checkLeaks ? qsTr("Off")
                 : !known ? qsTr("Checking…") : ""
        numberColour: instead !== "" ? palette.text : count > 0 ? overview.red : overview.green
        onClicked: {
            if (instead !== "" && state === "failed")
                VaultController.checkLeaksNow()
            else if (instead !== "" && state === "off")
                overview.leakSettingRequested()
            else
                overview.filterChosen("audit:" + list)
        }
    }
    // Seen on a light and a dark page alike.
    readonly property bool darkPage: palette.window.hslLightness < 0.5
    readonly property color red: darkPage ? "#ff6b6b" : "#c62828"
    readonly property color green: darkPage ? "#66bb6a" : "#2e7d32"
    readonly property color amber: darkPage ? "#ffb74d" : "#b26a00"
    readonly property color scoreColour: audit.score >= 90 ? green : audit.score >= 60 ? amber : red

    function countOf(filter) {
        return VaultController.revision >= 0 ? VaultController.items.countOf(filter) : 0
    }

    ColumnLayout {
        width: overview.availableWidth
        spacing: Math.round(16 * Appearance.factor)
        Label {
            objectName: "overviewName"
            text: VaultController.vaultName
            textFormat: Text.PlainText
            font.pointSize: Appearance.fontPointSize * 1.6
            elide: Text.ElideRight
            Layout.fillWidth: true
            Layout.margins: Math.round(20 * Appearance.factor)
            Layout.bottomMargin: 0
        }
        // The audit (D-065): the part of the passwords in none of its lists.
        ColumnLayout {
            objectName: "auditBlock"
            spacing: Math.round(8 * Appearance.factor)
            Layout.fillWidth: true
            Layout.maximumWidth: Math.round(640 * Appearance.factor)
            Layout.leftMargin: Math.round(20 * Appearance.factor)
            Layout.rightMargin: Math.round(20 * Appearance.factor)
            RowLayout {
                visible: overview.audit.score >= 0
                spacing: Math.round(12 * Appearance.factor)
                Label {
                    text: qsTr("Score")
                    font.bold: true
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: Math.round(10 * Appearance.factor)
                    radius: height / 2
                    color: Qt.rgba(overview.palette.text.r, overview.palette.text.g, overview.palette.text.b, 0.12)
                    Rectangle {
                        width: parent.width * Math.max(0, overview.audit.score) / 100
                        height: parent.height
                        radius: height / 2
                        color: overview.scoreColour
                    }
                }
                Label {
                    objectName: "auditScore"
                    text: qsTr("%1%").arg(overview.audit.score)
                    color: overview.scoreColour
                    font.bold: true
                    font.pointSize: Appearance.fontPointSize * 1.4
                }
            }
            GridLayout {
                columns: 2
                uniformCellWidths: true
                columnSpacing: Math.round(12 * Appearance.factor)
                rowSpacing: Math.round(12 * Appearance.factor)
                Layout.fillWidth: true
                AuditCard { text: qsTr("Leaked"); list: "leaked"; state: overview.audit.leakState }
                AuditCard { text: qsTr("Reused"); list: "reused" }
                AuditCard { text: qsTr("Weak"); list: "weak" }
                AuditCard { text: qsTr("Old"); list: "old" }
                AuditCard { text: qsTr("Breached websites"); list: "breached"; state: overview.audit.breachState }
            }
            AuditSource { visible: VaultController.checkLeaks }
        }
        Label {
            text: qsTr("Vault")
            font.bold: true
            Layout.leftMargin: Math.round(20 * Appearance.factor)
        }
        GridLayout {
            columns: 2
            uniformCellWidths: true
            columnSpacing: Math.round(12 * Appearance.factor)
            rowSpacing: Math.round(12 * Appearance.factor)
            Layout.fillWidth: true
            Layout.maximumWidth: Math.round(640 * Appearance.factor)
            Layout.leftMargin: Math.round(20 * Appearance.factor)
            Layout.rightMargin: Math.round(20 * Appearance.factor)
            Layout.bottomMargin: Math.round(20 * Appearance.factor)
            Count {
                objectName: "countItems"
                text: qsTr("Items")
                count: overview.countOf("")
                onClicked: overview.filterChosen("")
            }
            Count {
                objectName: "countFavorites"
                text: qsTr("Favorites")
                count: overview.countOf("favourites")
                onClicked: overview.filterChosen("favourites")
            }
            Count {
                objectName: "countCategories"
                text: qsTr("Categories")
                count: VaultController.revision >= 0 ? VaultController.categories().length : 0
                onClicked: overview.categoriesRequested()
            }
            Count {
                objectName: "countTags"
                text: qsTr("Tags")
                count: VaultController.revision >= 0 ? VaultController.tags().length : 0
                onClicked: overview.tagsRequested()
            }
            Count {
                objectName: "countArchive"
                text: qsTr("Archive")
                count: overview.countOf("archive")
                onClicked: overview.filterChosen("archive")
            }
            Count {
                objectName: "countTrash"
                text: qsTr("Trash")
                count: overview.countOf("trash")
                onClicked: overview.filterChosen("trash")
            }
        }
    }
}
