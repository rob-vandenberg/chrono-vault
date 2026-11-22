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
// VaultPage.qml 0.0.17

// --- Version history ----------------------------------------------------
// v0.0.17: Archive and Trash under Others; the chosen item with a grey fill and a blue bar; subtitles can be hidden
//          (D-033)
// v0.0.15: The vault at the top of the left column chooses among the person's vaults, makes or opens one (D-030)
// v0.0.14: The left column narrows the list: Home, Favorites, the categories and the tags as a tree; a new item from
//          any template (D-027, D-028)
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

    // A new item from a template (D-027).
    function newItem(templateId) {
        editor.whenFree(function() {
            itemList.currentIndex = -1
            chosenItemId = ""
            editor.edit(VaultController.newItemFrom(templateId), true)
        })
    }

    function newLogin() {
        newItem("login")
    }

    function newSecureNote() {
        newItem("note")
    }

    // File > New vault… and Open vault…, handled by the start page's windows.
    signal newVaultRequested()
    signal openVaultRequested()

    // Another of the person's vaults (D-030), after an edit not saved yet is
    // asked about.
    function switchVault(vaultFile) {
        editor.whenFree(function() {
            editor.close()
            chosenItemId = ""
            VaultController.switchToVault(vaultFile)
        })
    }

    // What the list is narrowed to (D-027, D-028); asks first about an edit
    // not saved yet, as choosing another item does.
    function choose(filter) {
        editor.whenFree(function() {
            editor.close()
            VaultController.filter = filter
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
        // Straight into the Trash, where Restore brings it back (D-033).
        editor.whenFree(function() {
            if (editor.itemId === id)
                editor.close()
            VaultController.moveToTrash(id, true)
        })
    }

    // Whether the item is still shown in the list's view: there, and not in
    // the Trash or the Archive unless that is what is shown (D-033).
    function stillShown(id) {
        const shown = VaultController.item(id)
        if (shown.id === undefined)
            return false
        if (shown.trashed)
            return VaultController.filter === "trash"
        if (shown.archived)
            return VaultController.filter === "archive"
        return VaultController.filter !== "trash" && VaultController.filter !== "archive"
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
            if (editor.itemId === "" && vaultPage.chosenItemId !== "" && !vaultPage.stillShown(vaultPage.chosenItemId))
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

        // Left: the vault, then Home (all items), Favorites, the categories
        // and the tags, each narrowing the list (D-027, D-028).
        Pane {
            id: vaultColumn
            objectName: "vaultColumn"
            SplitView.preferredWidth: Math.round(220 * Appearance.factor)
            SplitView.minimumWidth: Math.round(160 * Appearance.factor)
            padding: Math.round(12 * Appearance.factor)

            readonly property var categoryList: VaultController.revision >= 0 ? VaultController.categories() : []
            readonly property var tagList: VaultController.revision >= 0 ? VaultController.tags() : []

            ScrollView {
                id: columnScroll
                anchors.fill: parent
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: columnScroll.availableWidth
                    spacing: Math.round(2 * Appearance.factor)

                    // The vault shown; it opens the list of the person's
                    // vaults, and New vault and Open vault (D-030).
                    ItemDelegate {
                        id: vaultChooser
                        objectName: "vaultChooser"
                        Layout.fillWidth: true
                        Layout.bottomMargin: Math.round(14 * Appearance.factor)
                        onClicked: vaultMenu.popup(this, 0, height)
                        contentItem: RowLayout {
                            spacing: Math.round(10 * Appearance.factor)
                            Image {
                                source: "qrc:/icons/chrono-vault.svg"
                                sourceSize: Qt.size(Math.round(36 * Appearance.factor), Math.round(36 * Appearance.factor))
                                Layout.preferredWidth: Math.round(36 * Appearance.factor)
                                Layout.preferredHeight: Math.round(36 * Appearance.factor)
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
                            Label {
                                text: "▾"
                                opacity: 0.7
                            }
                        }
                        Menu {
                            id: vaultMenu
                            objectName: "vaultMenu"
                            Instantiator {
                                model: VaultController.vaults
                                delegate: MenuItem {
                                    required property string modelData
                                    text: modelData === VaultController.primaryVault
                                          ? qsTr("%1 (primary)").arg(VaultController.vaultNameOf(modelData))
                                          : VaultController.vaultNameOf(modelData)
                                    checkable: true
                                    checked: modelData === VaultController.vaultPath
                                    onTriggered: {
                                        checked = Qt.binding(function() { return modelData === VaultController.vaultPath })
                                        vaultPage.switchVault(modelData)
                                    }
                                }
                                onObjectAdded: function(index, object) { vaultMenu.insertItem(index, object) }
                                onObjectRemoved: function(index, object) { vaultMenu.removeItem(object) }
                            }
                            MenuSeparator {}
                            Action {
                                text: qsTr("New vault…")
                                onTriggered: vaultPage.newVaultRequested()
                            }
                            Action {
                                text: qsTr("Open vault…")
                                onTriggered: vaultPage.openVaultRequested()
                            }
                        }
                    }

                    ItemDelegate {
                        objectName: "homeEntry"
                        text: qsTr("Home")
                        icon.source: "qrc:/icons/home.svg"
                        icon.color: vaultPage.palette.buttonText
                        highlighted: VaultController.filter === ""
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("")
                    }
                    ItemDelegate {
                        objectName: "favouritesEntry"
                        text: qsTr("Favorites")
                        icon.source: "qrc:/icons/star.svg"
                        icon.color: vaultPage.palette.buttonText
                        highlighted: VaultController.filter === "favourites"
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("favourites")
                    }

                    Label {
                        text: qsTr("Categories")
                        font.bold: true
                        opacity: 0.7
                        Layout.topMargin: Math.round(14 * Appearance.factor)
                        Layout.leftMargin: Math.round(8 * Appearance.factor)
                    }
                    Repeater {
                        model: vaultColumn.categoryList.filter(function(category) { return !category.hidden })
                        delegate: ItemDelegate {
                            required property var modelData
                            objectName: "categoryEntry_" + modelData.id
                            text: modelData.name
                            icon.source: modelData.icon
                            icon.color: "transparent"
                            highlighted: VaultController.filter === "category:" + modelData.id
                            Layout.fillWidth: true
                            onClicked: vaultPage.choose("category:" + modelData.id)
                        }
                    }

                    Label {
                        text: qsTr("Tags")
                        font.bold: true
                        opacity: 0.7
                        Layout.topMargin: Math.round(14 * Appearance.factor)
                        Layout.leftMargin: Math.round(8 * Appearance.factor)
                    }
                    Label {
                        visible: vaultColumn.tagList.length === 0
                        text: qsTr("No tags yet.")
                        opacity: 0.6
                        Layout.leftMargin: Math.round(8 * Appearance.factor)
                    }
                    Repeater {
                        model: vaultColumn.tagList
                        delegate: ItemDelegate {
                            id: tagEntry
                            required property var modelData
                            objectName: "tagEntry_" + modelData.tag
                            text: modelData.name
                            icon.source: "qrc:/icons/tag.svg"
                            icon.color: vaultPage.palette.buttonText
                            // A nested tag stands under the one above it.
                            leftPadding: Math.round((8 + 16 * modelData.depth) * Appearance.factor)
                            highlighted: VaultController.filter === "tag:" + modelData.tag
                            Layout.fillWidth: true
                            onClicked: vaultPage.choose("tag:" + modelData.tag)
                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: tagMenu.popup()
                            }
                            Menu {
                                id: tagMenu
                                Action {
                                    text: qsTr("Rename tag…")
                                    onTriggered: tagDialog.ask(tagEntry.modelData.tag)
                                }
                                Action {
                                    text: qsTr("Remove tag")
                                    onTriggered: VaultController.removeTag(tagEntry.modelData.tag)
                                }
                            }
                        }
                    }

                    // The Archive and the Trash (D-033).
                    Label {
                        text: qsTr("Others")
                        font.bold: true
                        opacity: 0.7
                        Layout.topMargin: Math.round(14 * Appearance.factor)
                        Layout.leftMargin: Math.round(8 * Appearance.factor)
                    }
                    ItemDelegate {
                        objectName: "archiveEntry"
                        text: qsTr("Archive")
                        highlighted: VaultController.filter === "archive"
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("archive")
                    }
                    ItemDelegate {
                        objectName: "trashEntry"
                        text: qsTr("Trash")
                        highlighted: VaultController.filter === "trash"
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("trash")
                    }
                }
            }
        }

        // Middle: the search, the order, and the items under letter headings.
        ColumnLayout {
            SplitView.preferredWidth: Math.round(300 * Appearance.factor)
            SplitView.minimumWidth: Math.round(200 * Appearance.factor)
            spacing: Math.round(4 * Appearance.factor)

            TextField {
                objectName: "searchField"
                placeholderText: qsTr("Search")
                text: VaultController.searchText
                onTextEdited: VaultController.searchText = text
                Layout.fillWidth: true
                Layout.topMargin: Math.round(8 * Appearance.factor)
                Layout.leftMargin: Math.round(8 * Appearance.factor)
                Layout.rightMargin: Math.round(8 * Appearance.factor)
            }

            RowLayout {
                spacing: Math.round(2 * Appearance.factor)
                Layout.leftMargin: Math.round(8 * Appearance.factor)
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
                    leftPadding: Math.round(12 * Appearance.factor)
                    topPadding: Math.round(6 * Appearance.factor)
                    bottomPadding: Math.round(2 * Appearance.factor)
                }

                delegate: ItemDelegate {
                    id: listEntry
                    width: ListView.view.width
                    // The chosen item: a grey fill with a blue bar at its left
                    // edge, as in the reference program (D-033).
                    readonly property bool chosen: ListView.isCurrentItem
                    readonly property color textColour: vaultPage.palette.text
                    background: Rectangle {
                        color: listEntry.chosen ? Qt.rgba(vaultPage.palette.text.r, vaultPage.palette.text.g,
                                                          vaultPage.palette.text.b, 0.10)
                               : listEntry.hovered ? Qt.rgba(vaultPage.palette.text.r, vaultPage.palette.text.g,
                                                             vaultPage.palette.text.b, 0.05) : "transparent"
                        radius: Math.round(4 * Appearance.factor)
                        Rectangle {
                            objectName: "chosenBar"
                            visible: listEntry.chosen
                            width: Math.round(3 * Appearance.factor)
                            height: parent.height - Math.round(12 * Appearance.factor)
                            anchors.verticalCenter: parent.verticalCenter
                            radius: width / 2
                            color: vaultPage.palette.highlight
                        }
                    }
                    contentItem: RowLayout {
                        spacing: Math.round(10 * Appearance.factor)
                        Image {
                            source: model.icon
                            sourceSize: Qt.size(Math.round(36 * Appearance.factor), Math.round(36 * Appearance.factor))
                            Layout.preferredWidth: Math.round(36 * Appearance.factor)
                            Layout.preferredHeight: Math.round(36 * Appearance.factor)
                        }
                        ColumnLayout {
                            spacing: Math.round(2 * Appearance.factor)
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
                                visible: VaultController.showSubtitles && text !== ""
                                text: model.subtitle
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
                    text: VaultController.searchText !== "" ? qsTr("Nothing found.")
                          : VaultController.filter !== "" ? qsTr("No items here.") : qsTr("No items yet.")
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
                    if (vaultPage.chosenItemId !== "" && !vaultPage.stillShown(vaultPage.chosenItemId))
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

    // Renames a tag and the tags nested under it, in every item (D-028).
    DialogWindow {
        id: tagDialog
        objectName: "tagDialog"
        memoryName: "renameTag"
        property string tag: ""
        title: qsTr("Rename tag")
        standardButtons: Dialog.Ok | Dialog.Cancel
        function ask(chosenTag) {
            tag = chosenTag
            tagName.text = chosenTag
            open()
            tagName.forceActiveFocus()
            tagName.selectAll()
        }
        ColumnLayout {
            Label {
                text: qsTr("A slash nests a tag under another, as in “Work/Servers”.")
                wrapMode: Text.Wrap
                Layout.preferredWidth: Math.round(360 * Appearance.factor)
            }
            TextField {
                id: tagName
                objectName: "tagNameField"
                Layout.fillWidth: true
                onAccepted: tagDialog.accept()
            }
        }
        onAccepted: VaultController.renameTag(tag, tagName.text)
    }
}
