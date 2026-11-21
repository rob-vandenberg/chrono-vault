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
// VaultPage.qml 0.0.3

// --- Version history ----------------------------------------------------
// v0.0.3: Initial release

// The unlocked vault: the items on the left, the chosen one on the right.
Item {
    id: vaultPage
    objectName: "vaultPage"

    function newLogin() {
        editor.whenFree(function() {
            itemList.currentIndex = -1
            editor.edit(VaultController.newLogin(), true)
        })
    }

    function newSecureNote() {
        editor.whenFree(function() {
            itemList.currentIndex = -1
            editor.edit(VaultController.newSecureNote(), true)
        })
    }

    function hasUnsavedEdit() {
        return editor.itemId !== "" && editor.changed
    }

    function whenEditorFree(action) {
        editor.whenFree(action)
    }

    onVisibleChanged: if (!visible) editor.close()

    // The item being edited stays highlighted while the list changes.
    Connections {
        target: VaultController.items
        function onModelReset() {
            itemList.currentIndex = editor.itemId === "" ? -1 : VaultController.items.rowOf(editor.itemId)
        }
    }

    // Locking keeps what is being typed: it is stored first (OR-15).
    Connections {
        target: VaultController
        function onLocking() {
            if (editor.itemId !== "" && editor.changed)
                editor.store()
        }
    }

    SplitView {
        anchors.fill: parent

        ColumnLayout {
            SplitView.preferredWidth: 300
            SplitView.minimumWidth: 200
            spacing: 4

            TextField {
                placeholderText: qsTr("Search")
                text: VaultController.searchText
                onTextEdited: VaultController.searchText = text
                Layout.fillWidth: true
                Layout.margins: 6
            }

            ListView {
                id: itemList
                model: VaultController.items
                clip: true
                currentIndex: -1
                Layout.fillWidth: true
                Layout.fillHeight: true

                delegate: ItemDelegate {
                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem
                    contentItem: ColumnLayout {
                        spacing: 2
                        Label {
                            text: model.title
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: model.isNote ? qsTr("Secure note") : model.subtitle
                            opacity: 0.7
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                    onClicked: {
                        const row = index
                        const id = model.itemId
                        editor.whenFree(function() {
                            itemList.currentIndex = row
                            editor.edit(VaultController.item(id), false)
                        })
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: itemList.count === 0
                    text: VaultController.searchText === "" ? qsTr("No items yet.") : qsTr("Nothing found.")
                    opacity: 0.7
                }
            }
        }

        ItemEditor {
            id: editor
            objectName: "itemEditor"
            SplitView.fillWidth: true
            onClosed: itemList.currentIndex = -1
            onStored: function(itemId) { itemList.currentIndex = VaultController.items.rowOf(itemId) }
        }
    }
}
