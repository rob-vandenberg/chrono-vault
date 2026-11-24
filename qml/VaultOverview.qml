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
// VaultOverview.qml 0.5.83

// --- Version history ----------------------------------------------------
// v0.5.83: Initial release (D-063)

// The right side while no item is focused: the vault's name and its counts,
// each a way to what it counts. The audit comes above the counts later
// (ROADMAP.md step 12).
ScrollView {
    id: overview
    signal filterChosen(string filter)
    signal customizeRequested()
    contentWidth: availableWidth

    component Count: AbstractButton {
        id: card
        property int count: 0
        Layout.fillWidth: true
        Layout.preferredWidth: Math.round(160 * Appearance.factor)
        padding: Math.round(14 * Appearance.factor)
        hoverEnabled: true
        Accessible.name: text + " " + count
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
                text: card.count
                font.pointSize: Appearance.fontPointSize * 1.8
                Layout.fillWidth: true
            }
        }
    }
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
                onClicked: overview.customizeRequested()
            }
            Count {
                objectName: "countTags"
                text: qsTr("Tags")
                count: VaultController.revision >= 0 ? VaultController.tags().length : 0
                onClicked: overview.customizeRequested()
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
