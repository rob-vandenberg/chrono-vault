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
// TagsPage.qml 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Exact sizes (D-069)
// v0.6.88: Initial release, out of CustomizePage.qml (D-066)

// Settings > Tags (D-063, D-066): those of the unlocked vault, renamed and
// removed as with a right-click in the sidebar. Kept in the vault at once.
ExactColumn {
    id: tagsPage
    objectName: "tagsPage"
    gap: 8 * Appearance.factor

    readonly property bool unlocked: VaultController.state === VaultController.Unlocked
    readonly property var tagList: VaultController.revision >= 0 ? VaultController.tags() : []

    Label {
        text: qsTr("Tags")
        font.pointSize: Appearance.fontPointSize * 1.4
    }
    Label {
        text: tagsPage.unlocked
              ? qsTr("The tags belong to the vault that is open. Changes here are kept in it at once.")
              : qsTr("Unlock a vault to change its tags.")
        wrapMode: Text.Wrap
        opacity: 0.8
        Layout.fillWidth: true
    }

    Label {
        visible: tagsPage.unlocked && tagsPage.tagList.length === 0
        text: qsTr("No tags yet.")
        opacity: 0.7
    }
    // Renamed and removed as with a right-click in the sidebar (D-063).
    Repeater {
        model: tagsPage.unlocked ? tagsPage.tagList : []
        delegate: ExactRow {
            id: tagRow
            required property var modelData
            objectName: "tagRow_" + modelData.tag
            Layout.fillWidth: true
            gap: 6 * Appearance.factor
            Label {
                text: tagRow.modelData.name
                textFormat: Text.PlainText
                elide: Text.ElideRight
                // A nested tag stands under the one above it.
                leftPadding: 16 * tagRow.modelData.depth * Appearance.factor
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("%n item(s)", "", tagRow.modelData.count)
                opacity: 0.7
            }
            Button {
                objectName: "tagRename_" + tagRow.modelData.tag
                text: qsTr("Rename…")
                onClicked: tagDialog.ask(tagRow.modelData.tag)
            }
            Button {
                objectName: "tagRemove_" + tagRow.modelData.tag
                text: qsTr("Remove")
                onClicked: VaultController.removeTag(tagRow.modelData.tag)
            }
        }
    }
    Item { Layout.fillHeight: true }

    TagDialog {
        id: tagDialog
        objectName: "settingsTagDialog"
    }
}
