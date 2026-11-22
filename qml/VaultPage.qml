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
// VaultPage.qml 0.0.9

// --- Version history ----------------------------------------------------
// v0.0.9: Three columns after the reference program: the vault, the items under letter headings, the chosen item
//         read-only with the editor behind the pencil (D-025)
// v0.0.5: Whoever waits for the editor hears when the person cancels
// v0.0.4: Each item's icon in the list
// v0.0.3: Initial release

// The unlocked vault in three columns (D-025): the vault on the left, its
// items in the middle, the chosen item on the right, read-only, with the
// editor behind the pencil.
Item {
    id: vaultPage
    objectName: "vaultPage"

    // The item shown or edited on the right; empty when none is chosen.
    property string chosenItemId: ""

    function newLogin() {
        editor.whenFree(function() {
            itemList.currentIndex = -1
            chosenItemId = ""
            editor.edit(VaultController.newLogin(), true)
        })
    }

    function newSecureNote() {
        editor.whenFree(function() {
            itemList.currentIndex = -1
            chosenItemId = ""
            editor.edit(VaultController.newSecureNote(), true)
        })
    }

    function showItem(id) {
        editor.whenFree(function() {
            editor.close()
            chosenItemId = id
            itemList.currentIndex = VaultController.items.rowOf(id)
        })
    }

    function editChosen() {
        if (chosenItemId !== "")
            editor.edit(VaultController.item(chosenItemId), false)
    }

    // Item > Delete item…: the question of the editor, about the chosen item.
    function deleteChosen() {
        const id = chosenItemId
        if (id === "")
            return
        editor.whenFree(function() {
            if (editor.itemId !== id)
                editor.edit(VaultController.item(id), false)
            editor.askDelete()
        })
    }

    function hasUnsavedEdit() {
        return editor.itemId !== "" && editor.changed
    }

    function storeEdit() {
        editor.store()
    }

    function whenEditorFree(action, abandoned) {
        editor.whenFree(action, abandoned)
    }

    onVisibleChanged: {
        if (!visible) {
            editor.close()
            chosenItemId = ""
        }
    }

    // The chosen item stays highlighted while the list changes; one that is
    // gone (deleted) is no longer shown.
    Connections {
        target: VaultController.items
        function onModelReset() {
            const id = editor.itemId !== "" ? editor.itemId : vaultPage.chosenItemId
            const row = id === "" ? -1 : VaultController.items.rowOf(id)
            itemList.currentIndex = row
            if (editor.itemId === "" && vaultPage.chosenItemId !== "" && VaultController.item(vaultPage.chosenItemId).id === undefined)
                vaultPage.chosenItemId = ""
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

        // Left: the vault, then Home (all items).
        Pane {
            objectName: "vaultColumn"
            SplitView.preferredWidth: 220
            SplitView.minimumWidth: 160
            padding: 12

            ColumnLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 16

                RowLayout {
                    spacing: 10
                    Layout.fillWidth: true
                    Image {
                        source: "qrc:/icons/chrono-vault.svg"
                        sourceSize: Qt.size(36, 36)
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                    }
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        Label {
                            objectName: "vaultName"
                            text: VaultController.vaultName
                            textFormat: Text.PlainText
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: qsTr("Local file")
                            opacity: 0.7
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                ItemDelegate {
                    objectName: "homeEntry"
                    text: qsTr("Home")
                    icon.source: "qrc:/icons/home.svg"
                    icon.color: vaultPage.palette.buttonText
                    highlighted: true
                    Layout.fillWidth: true
                }
            }
        }

        // Middle: the search, the order, and the items under letter headings.
        ColumnLayout {
            SplitView.preferredWidth: 300
            SplitView.minimumWidth: 200
            spacing: 4

            TextField {
                objectName: "searchField"
                placeholderText: qsTr("Search")
                text: VaultController.searchText
                onTextEdited: VaultController.searchText = text
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.leftMargin: 8
                Layout.rightMargin: 8
            }

            RowLayout {
                spacing: 2
                Layout.leftMargin: 8
                Label {
                    text: qsTr("Sort By:")
                    opacity: 0.7
                }
                ToolButton {
                    objectName: "sortButton"
                    text: qsTr("Title") + " ▾"
                    font.bold: true
                    onClicked: sortMenu.popup(this, 0, height)
                    Menu {
                        id: sortMenu
                        MenuItem {
                            text: qsTr("Title")
                            checkable: true
                            checked: true
                        }
                    }
                }
            }

            ListView {
                id: itemList
                objectName: "itemList"
                model: VaultController.items
                clip: true
                currentIndex: -1
                Layout.fillWidth: true
                Layout.fillHeight: true

                section.property: "initial"
                section.criteria: ViewSection.FullString
                section.delegate: Label {
                    required property string section
                    text: section
                    font.bold: true
                    opacity: 0.7
                    leftPadding: 12
                    topPadding: 6
                    bottomPadding: 2
                }

                delegate: ItemDelegate {
                    id: listEntry
                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem
                    // The text of the chosen item in the highlight's own colour.
                    readonly property color textColour: highlighted ? vaultPage.palette.highlightedText : vaultPage.palette.text
                    contentItem: RowLayout {
                        spacing: 10
                        Image {
                            source: model.icon
                            sourceSize: Qt.size(36, 36)
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                        }
                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true
                            Label {
                                text: model.title
                                textFormat: Text.PlainText
                                color: listEntry.textColour
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: model.isNote ? qsTr("Secure note") : model.subtitle
                                textFormat: Text.PlainText
                                color: listEntry.textColour
                                opacity: 0.7
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }
                    onClicked: vaultPage.showItem(model.itemId)
                }

                Label {
                    anchors.centerIn: parent
                    visible: itemList.count === 0
                    text: VaultController.searchText === "" ? qsTr("No items yet.") : qsTr("Nothing found.")
                    opacity: 0.7
                }
            }
        }

        // Right: nothing chosen, the chosen item read-only, or the editor.
        StackLayout {
            SplitView.fillWidth: true
            currentIndex: editor.itemId !== "" ? 2 : (vaultPage.chosenItemId !== "" ? 1 : 0)

            Label {
                text: qsTr("Choose an item, or make a new one.")
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                opacity: 0.7
            }
            ItemDetails {
                itemId: vaultPage.chosenItemId
                onEditRequested: vaultPage.editChosen()
            }
            ItemEditor {
                id: editor
                objectName: "itemEditor"
                // Closed: back to the item it showed, when it is still there; a
                // deleted item is no longer shown.
                onClosed: {
                    if (vaultPage.chosenItemId !== "" && VaultController.item(vaultPage.chosenItemId).id === undefined)
                        vaultPage.chosenItemId = ""
                    itemList.currentIndex = vaultPage.chosenItemId === "" ? -1
                                            : VaultController.items.rowOf(vaultPage.chosenItemId)
                }
                onStored: function(itemId) {
                    vaultPage.chosenItemId = itemId
                    itemList.currentIndex = VaultController.items.rowOf(itemId)
                }
            }
        }
    }
}
