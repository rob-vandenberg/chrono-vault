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
// VaultPage.qml 0.2.49

// --- Version history ----------------------------------------------------
// v0.2.49: The sidebar snaps after 80 pixels of travel each way (the owner)
// v0.2.48: The sidebar snaps after 100 pixels of travel each way (the owner)
// v0.2.47: Counts above 0 only, long names cut off before them; minimum 180; the sidebar snaps shut or open past 120 when dragged (the owner)
// v0.2.46: The sidebar on Base; the item list 5 pixels in at each side (the owner)
// v0.2.45: A red dot on an item a merge left a note on (D-054)
// v0.1.42: listItem (D-052)
// v0.1.41: Up, Down, Page Up, Page Down, Home and End show the item they move to (D-051)
// v0.0.39: Collapsed entries as tall as expanded, with larger icons (D-049)
// v0.0.27: The sidebar collapses to its icons; its groups fold; Archive and Trash with icons; wider splitters (D-042)
// v0.0.24: Hide Sidebar, the groups shown by choice, item counts, Archive and Show Passwords from the menu (D-039)
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
    // The editor is open.
    readonly property bool editing: editor.itemId !== ""
    // Item > Show Passwords: the shown item's sensitive fields as text, until
    // another item is chosen or the vault locks (D-039).
    property bool revealAll: false
    onChosenItemIdChanged: revealAll = false
    Connections {
        target: VaultController
        // Hide Sensitive switched: what Show Passwords showed is hidden again.
        function onHideSensitiveChanged() { vaultPage.revealAll = false }
        // A group hidden while one of its entries is chosen: back to Home, so
        // the list is never narrowed by something no longer shown (D-039).
        function onViewChanged() {
            const filter = VaultController.filter
            if ((!VaultController.showCategories && filter.startsWith("category:"))
                    || (!VaultController.showTags && filter.startsWith("tag:"))
                    || (!VaultController.showOthers && (filter === "archive" || filter === "trash")))
                vaultPage.choose("")
        }
    }

    // The sidebar collapsed to its icons (D-042).
    readonly property bool collapsed: VaultController.sidebarCollapsed
    readonly property int collapsedWidth: Math.round(64 * Appearance.factor)
    property int expandedWidth: Math.round(220 * Appearance.factor)
    function setCollapsed(on) {
        if (on === collapsed)
            return
        if (on)
            expandedWidth = vaultColumn.width
        VaultController.sidebarCollapsed = on
        vaultColumn.SplitView.preferredWidth = on ? collapsedWidth : expandedWidth
    }
    Component.onCompleted: if (collapsed) vaultColumn.SplitView.preferredWidth = collapsedWidth
    // The narrowest the expanded sidebar gets (the owner, 23 November 2026).
    readonly property int sidebarMinimum: Math.round(180 * Appearance.factor)
    // Dragging its splitter: the sidebar holds at its minimum, or at its icons,
    // while the pointer travels 80 pixels more; then it snaps shut or open, as
    // the collapse button does (the owner, 23 November 2026).
    readonly property int snapTravel: Math.round(80 * Appearance.factor)
    function dragSidebarTo(x) {
        if (!collapsed && x < sidebarMinimum - snapTravel) {
            setCollapsed(true)
            expandedWidth = sidebarMinimum
        } else if (collapsed && x > collapsedWidth + snapTravel) {
            expandedWidth = sidebarMinimum
            setCollapsed(false)
        }
    }

    // An entry of the left column: its icon alone while collapsed, its name
    // then shown on hover (D-042).
    component SideEntry: ItemDelegate {
        display: vaultPage.collapsed ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
        ToolTip.visible: vaultPage.collapsed && hovered
        ToolTip.text: text
        Layout.fillWidth: true
        // Collapsed, an entry is exactly as tall as expanded, and its icon
        // larger, centred in that height (the owner, D-049).
        implicitHeight: entryMeasure.implicitHeight
        icon.width: vaultPage.collapsed ? vaultPage.collapsedIconSize : Math.round(16 * Appearance.factor)
        icon.height: vaultPage.collapsed ? vaultPage.collapsedIconSize : Math.round(16 * Appearance.factor)
        topPadding: vaultPage.collapsed ? Math.floor((implicitHeight - icon.height) / 2) : padding
        bottomPadding: vaultPage.collapsed ? Math.ceil((implicitHeight - icon.height) / 2) : padding
        // The number of items under it, with View > Show Item Counts (D-039):
        // only above 0 (the owner). A long name ends in "…" before it, and
        // takes its room when no number is shown.
        property string countFilter: ""
        property bool counted: false
        readonly property int itemCount: counted && VaultController.revision >= 0
                                         ? VaultController.items.countOf(countFilter) : 0
        readonly property bool countShown: counted && VaultController.showItemCounts && !vaultPage.collapsed
                                           && itemCount > 0
        rightPadding: padding + (countShown ? countLabel.implicitWidth + Math.round(10 * Appearance.factor) : 0)
        Label {
            id: countLabel
            objectName: "count_" + parent.countFilter
            visible: parent.countShown
            text: parent.itemCount
            opacity: 0.6
            anchors.right: parent.right
            anchors.rightMargin: Math.round(10 * Appearance.factor)
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    readonly property int collapsedIconSize: Math.round(24 * Appearance.factor)
    // An expanded entry, never shown: its height is every entry's height.
    ItemDelegate {
        id: entryMeasure
        visible: false
        text: qsTr("Home") // any one line of text: only its height counts
        display: AbstractButton.TextBesideIcon
        icon.source: "qrc:/icons/home.svg"
    }
    // A group's heading: its triangle folds or unfolds the group, in both
    // states of the sidebar; the name only while expanded (D-042).
    component GroupHeading: RowLayout {
        id: heading
        property string title: ""
        property string key: ""
        property bool folded: false
        signal foldToggled()
        spacing: 0
        Layout.topMargin: Math.round(14 * Appearance.factor)
        Layout.fillWidth: true
        ToolButton {
            objectName: "foldButton_" + heading.key
            // Collapsed: the button spans the column, so its triangle stands
            // centered, as in the owner's mockup.
            Layout.fillWidth: vaultPage.collapsed
            text: heading.folded ? "▸" : "▾"
            opacity: 0.7
            implicitWidth: Math.round(24 * Appearance.factor)
            implicitHeight: Math.round(22 * Appearance.factor)
            onClicked: heading.foldToggled()
            ToolTip.visible: vaultPage.collapsed && hovered
            ToolTip.text: heading.title
        }
        Label {
            visible: !vaultPage.collapsed
            Layout.fillWidth: true
            text: heading.title
            font.bold: true
            opacity: 0.7
        }
    }

    // A number of items at the right of an entry of the left column, with
    // View > Show Item Counts (D-039).
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

    // The item list, which takes the keyboard back after a menu (D-052).
    function listItem() {
        return itemList
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

    // Item > Archive: the chosen item into the Archive (D-033), where
    // Unarchive brings it back.
    function archiveChosen() {
        const id = chosenItemId
        if (id === "")
            return
        editor.whenFree(function() {
            if (editor.itemId === id)
                editor.close()
            VaultController.archiveItem(id, true)
        })
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
        } else {
            // The keyboard leaves the hidden password field for the list, so
            // typing never goes into a page no longer shown, and the menus'
            // shortcuts work (D-039).
            itemList.forceActiveFocus()
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
        // 8 pixels to catch with the mouse (the owner); the line shown stays
        // as thin as before (D-042).
        handle: Item {
            id: splitHandle
            implicitWidth: Math.max(8, Math.round(8 * Appearance.factor))
            // The sidebar's own splitter, while held: where the pointer is
            // decides whether the sidebar snaps shut or open.
            Timer {
                interval: 16
                repeat: true
                running: splitHandle.SplitHandle.pressed && vaultColumn.visible
                         && splitHandle.x <= vaultColumn.width + 1
                onTriggered: vaultPage.dragSidebarTo(Appearance.pointerIn(vaultPage).x)
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 2
                height: parent.height
                color: parent.SplitHandle.pressed ? vaultPage.palette.dark
                       : parent.SplitHandle.hovered ? vaultPage.palette.midlight : vaultPage.palette.mid
            }
        }

        // Left: the vault, then Home (all items), Favorites, the categories
        // and the tags, each narrowing the list (D-027, D-028).
        Pane {
            id: vaultColumn
            objectName: "vaultColumn"
            // View > Hide Sidebar (D-039).
            visible: !VaultController.sidebarHidden
            SplitView.preferredWidth: Math.round(220 * Appearance.factor)
            // Collapsed: a narrow column of fixed width (D-042).
            SplitView.minimumWidth: vaultPage.collapsed ? vaultPage.collapsedWidth : vaultPage.sidebarMinimum
            SplitView.maximumWidth: vaultPage.collapsed ? vaultPage.collapsedWidth : Number.POSITIVE_INFINITY
            padding: vaultPage.collapsed ? Math.round(6 * Appearance.factor) : Math.round(12 * Appearance.factor)
            // The cards' own colour, the theme's Base (the owner, 23 November 2026).
            background: Rectangle { color: vaultPage.palette.base }

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

                    // Collapses the sidebar to its icons, or expands it (D-042).
                    ToolButton {
                        objectName: "sidebarToggle"
                        icon.source: "qrc:/icons/sidebar.svg"
                        icon.color: vaultPage.palette.buttonText
                        onClicked: vaultPage.setCollapsed(!vaultPage.collapsed)
                        ToolTip.visible: hovered
                        ToolTip.text: vaultPage.collapsed ? qsTr("Expand the sidebar") : qsTr("Collapse the sidebar")
                    }

                    // The vault shown; it opens the list of the person's
                    // vaults, and New vault and Open vault (D-030).
                    ItemDelegate {
                        id: vaultChooser
                        objectName: "vaultChooser"
                        Layout.fillWidth: true
                        Layout.bottomMargin: Math.round(14 * Appearance.factor)
                        onClicked: vaultMenu.popup(this, 0, height)
                        ToolTip.visible: vaultPage.collapsed && hovered
                        ToolTip.text: VaultController.vaultName
                        // Collapsed: the icon with its ▾ below it (D-042).
                        contentItem: GridLayout {
                            columns: vaultPage.collapsed ? 1 : 3
                            columnSpacing: Math.round(10 * Appearance.factor)
                            rowSpacing: 0
                            Image {
                                Layout.alignment: Qt.AlignHCenter
                                source: "qrc:/icons/chrono-vault.svg"
                                sourceSize: Qt.size(Math.round(36 * Appearance.factor), Math.round(36 * Appearance.factor))
                                Layout.preferredWidth: Math.round(36 * Appearance.factor)
                                Layout.preferredHeight: Math.round(36 * Appearance.factor)
                            }
                            ColumnLayout {
                                visible: !vaultPage.collapsed
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
                                Layout.alignment: Qt.AlignHCenter
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

                    SideEntry {
                        objectName: "homeEntry"
                        text: qsTr("Home")
                        icon.source: "qrc:/icons/home.svg"
                        icon.color: vaultPage.palette.buttonText
                        highlighted: VaultController.filter === ""
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("")
                        counted: true; countFilter: ""
                    }
                    SideEntry {
                        objectName: "favouritesEntry"
                        text: qsTr("Favorites")
                        icon.source: "qrc:/icons/star.svg"
                        icon.color: vaultPage.palette.buttonText
                        highlighted: VaultController.filter === "favourites"
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("favourites")
                        counted: true; countFilter: "favourites"
                    }

                    GroupHeading {
                        visible: VaultController.showCategories
                        title: qsTr("Categories")
                        key: "categories"
                        folded: VaultController.categoriesFolded
                        onFoldToggled: VaultController.categoriesFolded = !VaultController.categoriesFolded
                    }
                    Repeater {
                        model: VaultController.showCategories && !VaultController.categoriesFolded
                               ? vaultColumn.categoryList.filter(function(category) { return !category.hidden }) : []
                        delegate: SideEntry {
                            required property var modelData
                            objectName: "categoryEntry_" + modelData.id
                            text: modelData.name
                            icon.source: modelData.icon
                            icon.color: "transparent"
                            highlighted: VaultController.filter === "category:" + modelData.id
                            Layout.fillWidth: true
                            onClicked: vaultPage.choose("category:" + modelData.id)
                            counted: true
                            countFilter: "category:" + modelData.id
                        }
                    }

                    GroupHeading {
                        visible: VaultController.showTags
                        title: qsTr("Tags")
                        key: "tags"
                        folded: VaultController.tagsFolded
                        onFoldToggled: VaultController.tagsFolded = !VaultController.tagsFolded
                    }
                    Label {
                        visible: VaultController.showTags && !VaultController.tagsFolded && !vaultPage.collapsed
                                 && vaultColumn.tagList.length === 0
                        text: qsTr("No tags yet.")
                        opacity: 0.6
                        Layout.leftMargin: Math.round(8 * Appearance.factor)
                    }
                    Repeater {
                        model: VaultController.showTags && !VaultController.tagsFolded ? vaultColumn.tagList : []
                        delegate: SideEntry {
                            id: tagEntry
                            required property var modelData
                            objectName: "tagEntry_" + modelData.tag
                            text: modelData.name
                            icon.source: "qrc:/icons/tag.svg"
                            icon.color: vaultPage.palette.buttonText
                            // A nested tag stands under the one above it.
                            leftPadding: Math.round((8 + (vaultPage.collapsed ? 0 : 16 * modelData.depth)) * Appearance.factor)
                            highlighted: VaultController.filter === "tag:" + modelData.tag
                            Layout.fillWidth: true
                            onClicked: vaultPage.choose("tag:" + modelData.tag)
                            counted: true; countFilter: "tag:" + tagEntry.modelData.tag
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
                    GroupHeading {
                        visible: VaultController.showOthers
                        title: qsTr("Others")
                        key: "others"
                        folded: VaultController.othersFolded
                        onFoldToggled: VaultController.othersFolded = !VaultController.othersFolded
                    }
                    SideEntry {
                        objectName: "archiveEntry"
                        visible: VaultController.showOthers && !VaultController.othersFolded
                        text: qsTr("Archive")
                        icon.source: "qrc:/icons/archive.svg"
                        icon.color: vaultPage.palette.buttonText
                        highlighted: VaultController.filter === "archive"
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("archive")
                        counted: true; countFilter: "archive"
                    }
                    SideEntry {
                        objectName: "trashEntry"
                        visible: VaultController.showOthers && !VaultController.othersFolded
                        text: qsTr("Trash")
                        icon.source: "qrc:/icons/trash.svg"
                        icon.color: vaultPage.palette.buttonText
                        highlighted: VaultController.filter === "trash"
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("trash")
                        counted: true; countFilter: "trash"
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
                Layout.leftMargin: Math.round(13 * Appearance.factor)
                Layout.rightMargin: Math.round(13 * Appearance.factor)
            }

            RowLayout {
                spacing: Math.round(2 * Appearance.factor)
                Layout.leftMargin: Math.round(13 * Appearance.factor)
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
                boundsBehavior: Flickable.StopAtBounds // no bounce (the owner, D-046)
                objectName: "itemList"
                model: VaultController.items
                clip: true
                currentIndex: -1
                Layout.fillWidth: true
                Layout.fillHeight: true
                // 5 pixels more at the left and right (the owner, 23 November 2026).
                Layout.leftMargin: Math.round(5 * Appearance.factor)
                Layout.rightMargin: Math.round(5 * Appearance.factor)
                // Up, Down, Page Up, Page Down, Home and End show the item they
                // move to, as a click does, an edit not saved yet asked about
                // first (D-051).
                Keys.onPressed: function(event) {
                    if (event.modifiers !== Qt.NoModifier || count === 0)
                        return
                    const page = Math.max(1, Math.floor(height / Math.max(1, currentItem ? currentItem.height : 40)) - 1)
                    let row = currentIndex
                    switch (event.key) {
                    case Qt.Key_Up: row = currentIndex < 0 ? 0 : currentIndex - 1; break
                    case Qt.Key_Down: row = currentIndex + 1; break
                    case Qt.Key_PageUp: row = currentIndex - page; break
                    case Qt.Key_PageDown: row = currentIndex + page; break
                    case Qt.Key_Home: row = 0; break
                    case Qt.Key_End: row = count - 1; break
                    default: return
                    }
                    event.accepted = true
                    row = Math.max(0, Math.min(count - 1, row))
                    if (row !== currentIndex)
                        vaultPage.showItem(VaultController.items.idAt(row))
                }

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
                        // Something a merge left to settle (D-054).
                        Rectangle {
                            objectName: "mergedMark"
                            visible: model.merged
                            implicitWidth: Math.round(8 * Appearance.factor)
                            implicitHeight: implicitWidth
                            radius: width / 2
                            color: "#d32f2f"
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
                revealAll: vaultPage.revealAll
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
