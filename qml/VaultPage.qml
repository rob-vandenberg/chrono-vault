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
// VaultPage.qml 0.5.82

// --- Version history ----------------------------------------------------
// v0.5.82: The list by mouse and keys as a person guesses, several items, Delete asked, the sidebar's keys, Ctrl+F (D-062)
// v0.5.78: Texts in dialogs wrap with the window (D-061)
// v0.5.76: The item's sync conflict to the Sync conflicts dialog (D-060)
// v0.3.60: Under the vault's name: WebDAV for a vault kept there, and why its last sync failed (D-057)
// v0.2.56: A right-click on a category offers Edit… (the owner, D-056)
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
    onChosenItemIdChanged: {
        revealAll = false
        leftOutId = ""
        if (chosenItemId !== "")
            selectedIds = [chosenItemId]
        else if (selectedIds.length === 1)
            selectedIds = []
    }
    // The items chosen together (D-062): one, the item shown at the right, or
    // several, of which the right side says how many. The anchor is where a
    // range begins.
    property var selectedIds: []
    property string anchorId: ""
    property string leftOutId: ""
    readonly property bool several: selectedIds.length > 1
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
        // A click gives the sidebar the keyboard (D-062).
        readonly property bool sideEntry: true
        focusPolicy: Qt.NoFocus
        onPressed: sideKeys.forceActiveFocus(Qt.MouseFocusReason)
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
            selectedIds = []
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
    // The item's sync conflict, to be settled in the Sync conflicts dialog (D-060).
    signal conflictRequested(string itemId)

    // Another of the person's vaults (D-030), after an edit not saved yet is
    // asked about.
    function switchVault(vaultFile) {
        editor.whenFree(function() {
            editor.close()
            chosenItemId = ""
            selectedIds = []
            VaultController.switchToVault(vaultFile)
        })
    }

    // What the list is narrowed to (D-027, D-028); asks first about an edit
    // not saved yet, as choosing another item does.
    function choose(filter) {
        editor.whenFree(function() {
            editor.close()
            if (several)
                selectedIds = []
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
            selectedIds = [id]
            anchorId = id
            showRow(VaultController.items.rowOf(id))
        })
    }

    // The list's mark on this row, in view.
    function showRow(row) {
        itemList.currentIndex = row
        if (row >= 0)
            itemList.positionViewAtIndex(row, ListView.Contain)
    }

    // Several items chosen together, the mark on the last one touched.
    function selectItems(ids, markId) {
        editor.whenFree(function() {
            editor.close()
            chosenItemId = ids.length === 1 ? ids[0] : ""
            selectedIds = ids
            // Never on an item just taken away.
            const mark = ids.indexOf(markId) >= 0 ? markId : ids.length > 0 ? ids[ids.length - 1] : ""
            showRow(mark === "" ? -1 : VaultController.items.rowOf(mark))
        })
    }
    function toggleItem(id) {
        const at = selectedIds.indexOf(id)
        const ids = at >= 0 ? selectedIds.filter(function(one) { return one !== id }) : selectedIds.concat([id])
        anchorId = id
        selectItems(ids, id)
    }
    function selectRangeTo(row) {
        const items = VaultController.items
        let from = items.rowOf(anchorId)
        if (from < 0) {
            from = row
            anchorId = items.idAt(row)
        }
        const ids = []
        for (let one = Math.min(from, row); one <= Math.max(from, row); ++one)
            ids.push(items.idAt(one))
        selectItems(ids, items.idAt(row))
    }
    function selectAll() {
        const ids = []
        for (let row = 0; row < itemList.count; ++row)
            ids.push(VaultController.items.idAt(row))
        if (ids.length > 0)
            selectItems(ids, VaultController.items.idAt(Math.max(0, itemList.currentIndex)))
    }
    readonly property bool inTrash: VaultController.filter === "trash"
    // What the chosen items are, for the menus.
    readonly property var chosenItems: VaultController.revision >= 0
                                       ? selectedIds.map(function(id) { return VaultController.item(id) })
                                                    .filter(function(one) { return one.id !== undefined }) : []
    readonly property bool anyTrashed: chosenItems.some(function(one) { return one.trashed === true })
    readonly property bool allArchived: chosenItems.length > 0
                                        && chosenItems.every(function(one) { return one.archived === true })
    readonly property bool allFavourite: chosenItems.length > 0
                                         && chosenItems.every(function(one) { return one.favourite === true })
    // Done to the chosen items; then the one after them is chosen, so the
    // next key acts on what the person sees.
    function actOnChosen(action) {
        actOn(selectedIds.slice(), action)
    }
    function actOn(ids, action) {
        if (ids.length === 0)
            return
        const items = VaultController.items
        let last = -1
        for (const id of ids)
            last = Math.max(last, items.rowOf(id))
        let next = ""
        for (let row = last + 1; row < itemList.count && next === ""; ++row) {
            if (ids.indexOf(items.idAt(row)) < 0)
                next = items.idAt(row)
        }
        for (let row = last; row >= 0 && next === ""; --row) {
            if (ids.indexOf(items.idAt(row)) < 0)
                next = items.idAt(row)
        }
        action(ids)
        const shown = ids.filter(function(id) { return items.rowOf(id) >= 0 })
        if (shown.length > 0)
            return // still listed, such as a favorite
        selectedIds = []
        chosenItemId = ""
        if (next !== "" && items.rowOf(next) >= 0)
            showItem(next)
    }
    function favouriteChosen() {
        const on = !allFavourite
        actOnChosen(function(ids) { VaultController.placeItems(ids, "favourite", on) })
    }
    function restoreChosen() {
        actOnChosen(function(ids) { VaultController.placeItems(ids, "trashed", false) })
    }
    // What the question is about: one title, or how many.
    function nameOfChosen() {
        return several ? qsTr("%n item(s)", "", selectedIds.length)
                       : qsTr("“%1”").arg(chosenItems.length > 0 ? chosenItems[0].title : "")
    }

    // Enter or a double click on the item: its editor; not in the Trash.
    function editShown() {
        if (!several && chosenItemId !== "" && !editing && !anyTrashed)
            editChosen()
    }
    function askRestore() {
        if (several)
            placeQuestion.ask(qsTr("Restore %1?").arg(nameOfChosen()), function() { vaultPage.restoreChosen() })
        else
            restoreChosen()
    }

    function editChosen() {
        if (chosenItemId !== "")
            editor.edit(VaultController.item(chosenItemId), false)
    }

    // Item > Archive: the chosen items into the Archive (D-033), where
    // Unarchive brings them back.
    function archiveChosen() {
        const on = !allArchived
        const go = function() {
            editor.whenFree(function() {
                editor.close()
                actOnChosen(function(ids) { VaultController.placeItems(ids, "archived", on) })
            })
        }
        if (several)
            placeQuestion.ask(on ? qsTr("Archive %1?").arg(nameOfChosen()) : qsTr("Unarchive %1?").arg(nameOfChosen()), go)
        else if (selectedIds.length === 1)
            go()
    }

    // Delete, Backspace and Item > Move to Trash: asked first (D-062); in the
    // Trash, about deleting for good.
    function deleteChosen() {
        if (selectedIds.length === 0)
            return
        editor.whenFree(function() {
            editor.close()
            if (anyTrashed)
                foreverQuestion.ask(selectedIds.slice(), nameOfChosen())
            else
                placeQuestion.ask(qsTr("Move %1 to the Trash?").arg(nameOfChosen()), function() {
                    vaultPage.actOnChosen(function(ids) { VaultController.placeItems(ids, "trashed", true) })
                })
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
            selectedIds = []
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
            // An item the search left out is shown again with the search gone.
            if (vaultPage.leftOutId !== "" && vaultPage.chosenItemId === "" && !vaultPage.several && editor.itemId === ""
                    && VaultController.items.rowOf(vaultPage.leftOutId) >= 0)
                vaultPage.chosenItemId = vaultPage.leftOutId
            const id = editor.itemId !== "" ? editor.itemId : vaultPage.chosenItemId
            const row = id === "" ? -1 : VaultController.items.rowOf(id)
            itemList.currentIndex = row
            if (editor.itemId === "" && vaultPage.chosenItemId !== ""
                    && (!vaultPage.stillShown(vaultPage.chosenItemId)
                        || (row < 0 && VaultController.searchText !== ""))) {
                const out = VaultController.searchText !== "" && vaultPage.stillShown(vaultPage.chosenItemId)
                            ? vaultPage.chosenItemId : ""
                vaultPage.chosenItemId = ""
                vaultPage.leftOutId = out
            }
            if (vaultPage.several) {
                const left = vaultPage.selectedIds.filter(function(one) { return VaultController.items.rowOf(one) >= 0 })
                vaultPage.selectedIds = left
                if (left.length === 1)
                    vaultPage.chosenItemId = left[0]
                if (left.length > 0)
                    itemList.currentIndex = VaultController.items.rowOf(left[left.length - 1])
            }
        }
    }

    // Ctrl+F goes to the search field (D-062).
    Shortcut {
        sequences: [StandardKey.Find]
        enabled: vaultPage.visible && Appearance.focusWindow === vaultPage.Window.window
        onActivated: {
            searchField.forceActiveFocus(Qt.ShortcutFocusReason)
            searchField.selectAll()
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

            // Holds the keyboard for the sidebar (D-062): the keys choose a
            // line, whose items show at once; the headings are skipped.
            Item {
                id: sideKeys
                objectName: "sideKeys"
                activeFocusOnTab: true
                function entries() {
                    const found = []
                    for (let i = 0; i < sideColumn.children.length; ++i) {
                        const one = sideColumn.children[i]
                        if (one.sideEntry === true && one.visible)
                            found.push(one)
                    }
                    found.sort(function(a, b) { return a.y - b.y })
                    return found
                }
                Keys.onPressed: function(event) {
                    if ((event.modifiers & ~Qt.KeypadModifier) !== Qt.NoModifier)
                        return
                    const all = entries()
                    if (all.length === 0)
                        return
                    let at = all.findIndex(function(one) { return one.countFilter === VaultController.filter })
                    const page = Math.max(1, Math.floor(columnScroll.height / Math.max(1, all[0].height)) - 1)
                    switch (event.key) {
                    case Qt.Key_Up: at = at - 1; break
                    case Qt.Key_Down: at = at + 1; break
                    case Qt.Key_PageUp: at = at - page; break
                    case Qt.Key_PageDown: at = at + page; break
                    case Qt.Key_Home: at = 0; break
                    case Qt.Key_End: at = all.length - 1; break
                    default: return
                    }
                    event.accepted = true
                    const chosen = all[Math.max(0, Math.min(all.length - 1, at))]
                    if (chosen.countFilter !== VaultController.filter)
                        vaultPage.choose(chosen.countFilter)
                    const view = columnScroll.contentItem
                    if (chosen.y < view.contentY)
                        view.contentY = chosen.y
                    else if (chosen.y + chosen.height > view.contentY + view.height)
                        view.contentY = chosen.y + chosen.height - view.height
                }
            }

            ScrollView {
                id: columnScroll
                anchors.fill: parent
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    id: sideColumn
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
                                    objectName: "vaultPlace"
                                    text: VaultController.onWebDav ? qsTr("WebDAV") : qsTr("Local file")
                                    opacity: 0.7
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                // Why the last sync failed; gone after the next good one (D-057).
                                Label {
                                    objectName: "syncProblem"
                                    visible: VaultController.syncProblem !== ""
                                    text: VaultController.syncProblem
                                    color: "#d32f2f"
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 3
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
                            id: categoryEntry
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
                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: categoryMenu.popup()
                            }
                            Menu {
                                id: categoryMenu
                                objectName: "categoryMenu_" + categoryEntry.modelData.id
                                Action {
                                    text: qsTr("Edit…")
                                    onTriggered: {
                                        sidebarCategoryEditor.active = true
                                        sidebarCategoryEditor.item.edit(categoryEntry.modelData)
                                    }
                                }
                            }
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
                id: searchField
                objectName: "searchField"
                // Down and Enter go to the list, Esc clears it and goes there
                // too (D-062).
                function toList() {
                    itemList.forceActiveFocus(Qt.TabFocusReason)
                    if (itemList.currentIndex < 0 && itemList.count > 0)
                        vaultPage.showItem(VaultController.items.idAt(0))
                }
                Keys.onDownPressed: toList()
                onAccepted: toList()
                Keys.onEscapePressed: {
                    VaultController.searchText = ""
                    itemList.forceActiveFocus(Qt.TabFocusReason)
                }
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

            // The Trash as a whole (D-062).
            RowLayout {
                visible: vaultPage.inTrash
                spacing: Math.round(6 * Appearance.factor)
                Layout.leftMargin: Math.round(13 * Appearance.factor)
                readonly property bool any: VaultController.revision >= 0 && VaultController.items.countOf("trash") > 0
                Button {
                    objectName: "restoreAllButton"
                    text: qsTr("Restore all")
                    enabled: parent.any
                    onClicked: {
                        const ids = VaultController.trashedItems()
                        placeQuestion.ask(qsTr("Restore %1?").arg(qsTr("%n item(s)", "", ids.length)), function() {
                            vaultPage.chosenItemId = ""
                            vaultPage.selectedIds = []
                            VaultController.placeItems(ids, "trashed", false)
                        })
                    }
                }
                Button {
                    objectName: "emptyTrashButton"
                    text: qsTr("Empty Trash…")
                    enabled: parent.any
                    onClicked: {
                        const ids = VaultController.trashedItems()
                        foreverQuestion.ask(ids, qsTr("%n item(s)", "", ids.length))
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
                activeFocusOnTab: true
                keyNavigationEnabled: false
                highlightMoveDuration: 0 // the view is there at once, however far
                property string typed: ""
                Timer {
                    id: typedEnds
                    interval: 1000
                    onTriggered: itemList.typed = ""
                }
                // Up, Down, Page Up, Page Down, Home and End show the item they
                // move to, as a click does, an edit not saved yet asked about
                // first (D-051); with Shift they choose a range, and letters
                // jump to the item that starts with them (D-062).
                Keys.onPressed: function(event) {
                    const modifiers = event.modifiers & ~Qt.KeypadModifier
                    const shift = modifiers === Qt.ShiftModifier
                    if (modifiers === Qt.ControlModifier && event.key === Qt.Key_A) {
                        event.accepted = true
                        vaultPage.selectAll()
                        return
                    }
                    if ((modifiers !== Qt.NoModifier && !shift) || count === 0)
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
                    case Qt.Key_Return:
                    case Qt.Key_Enter:
                        if (!shift) {
                            event.accepted = true
                            vaultPage.editShown()
                        }
                        return
                    default:
                        if (event.text.length === 1 && event.text > " " || (event.text === " " && typed !== "")) {
                            event.accepted = true
                            typed += event.text
                            typedEnds.restart()
                            const found = VaultController.items.rowStartingWith(typed, currentIndex, typed.length > 1)
                            if (found >= 0)
                                vaultPage.showItem(VaultController.items.idAt(found))
                        }
                        return
                    }
                    event.accepted = true
                    row = Math.max(0, Math.min(count - 1, row))
                    if (shift)
                        vaultPage.selectRangeTo(row)
                    else if (row !== currentIndex || vaultPage.several)
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
                    readonly property bool chosen: vaultPage.several
                                                   ? vaultPage.selectedIds.indexOf(model.itemId) >= 0
                                                   : ListView.isCurrentItem
                    objectName: "itemRow_" + index
                    focusPolicy: Qt.NoFocus
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
                    // A click gives the list the keyboard; Ctrl adds or takes
                    // one away, Shift takes a range (D-062).
                    property int heldKeys: Qt.NoModifier
                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        gesturePolicy: TapHandler.DragThreshold
                        onPressedChanged: if (pressed) listEntry.heldKeys = point.modifiers
                    }
                    onClicked: {
                        itemList.forceActiveFocus(Qt.MouseFocusReason)
                        if (heldKeys & Qt.ControlModifier)
                            vaultPage.toggleItem(model.itemId)
                        else if (heldKeys & Qt.ShiftModifier)
                            vaultPage.selectRangeTo(index)
                        else
                            vaultPage.showItem(model.itemId)
                    }
                    onDoubleClicked: vaultPage.editShown()
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: {
                            itemList.forceActiveFocus(Qt.MouseFocusReason)
                            if (vaultPage.selectedIds.indexOf(model.itemId) < 0)
                                vaultPage.showItem(model.itemId)
                            if (vaultPage.selectedIds.indexOf(model.itemId) >= 0)
                                listMenu.popup()
                        }
                    }
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
            currentIndex: editor.itemId !== "" ? 2 : vaultPage.several ? 3 : (vaultPage.chosenItemId !== "" ? 1 : 0)

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
                onTrashRequested: vaultPage.deleteChosen()
                onRestoreRequested: vaultPage.restoreChosen()
                onArchiveRequested: vaultPage.archiveChosen()
                onConflictRequested: function(itemId) { vaultPage.conflictRequested(itemId) }
            }
            ItemEditor {
                id: editor
                objectName: "itemEditor"
                // Closed: back to the item it showed, when it is still there; a
                // deleted item is no longer shown.
                onClosed: function(wasOpen) {
                    if (vaultPage.chosenItemId !== "" && !vaultPage.stillShown(vaultPage.chosenItemId))
                        vaultPage.chosenItemId = ""
                    itemList.currentIndex = vaultPage.chosenItemId === "" ? -1
                                            : VaultController.items.rowOf(vaultPage.chosenItemId)
                    // The editor held the keyboard: back to the list.
                    if (wasOpen && vaultPage.visible)
                        itemList.forceActiveFocus()
                }
                onStored: function(itemId) {
                    vaultPage.chosenItemId = itemId
                    itemList.currentIndex = VaultController.items.rowOf(itemId)
                }
            }

            // Several items chosen together (D-062).
            ColumnLayout {
                objectName: "severalPage"
                spacing: Math.round(12 * Appearance.factor)
                Item { Layout.fillHeight: true }
                Label {
                    objectName: "severalLabel"
                    text: qsTr("%1 items selected").arg(vaultPage.selectedIds.length)
                    font.pointSize: Appearance.fontPointSize * 1.3
                    Layout.alignment: Qt.AlignHCenter
                }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Math.round(6 * Appearance.factor)
                    Button {
                        objectName: "severalFavourite"
                        visible: !vaultPage.anyTrashed
                        text: vaultPage.allFavourite ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
                        onClicked: vaultPage.favouriteChosen()
                    }
                    Button {
                        objectName: "severalArchive"
                        visible: !vaultPage.anyTrashed
                        text: vaultPage.allArchived ? qsTr("Unarchive") : qsTr("Archive")
                        onClicked: vaultPage.archiveChosen()
                    }
                    Button {
                        objectName: "severalTrash"
                        visible: !vaultPage.anyTrashed
                        text: qsTr("Move to Trash")
                        onClicked: vaultPage.deleteChosen()
                    }
                    Button {
                        objectName: "severalRestore"
                        visible: vaultPage.anyTrashed
                        text: qsTr("Restore")
                        onClicked: vaultPage.askRestore()
                    }
                    Button {
                        objectName: "severalDelete"
                        visible: vaultPage.anyTrashed
                        text: qsTr("Delete Permanently…")
                        onClicked: vaultPage.deleteChosen()
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }

    // An item's right-click menu (D-062).
    Menu {
        id: listMenu
        objectName: "itemListMenu"
        MenuItem {
            objectName: "listMenuEdit"
            text: qsTr("Edit")
            visible: !vaultPage.several && !vaultPage.anyTrashed
            height: visible ? implicitHeight : 0
            onTriggered: vaultPage.editShown()
        }
        MenuItem {
            text: vaultPage.allFavourite ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
            visible: !vaultPage.anyTrashed
            height: visible ? implicitHeight : 0
            onTriggered: vaultPage.favouriteChosen()
        }
        MenuItem {
            text: vaultPage.allArchived ? qsTr("Unarchive") : qsTr("Archive")
            visible: !vaultPage.anyTrashed
            height: visible ? implicitHeight : 0
            onTriggered: vaultPage.archiveChosen()
        }
        MenuItem {
            objectName: "listMenuTrash"
            text: qsTr("Move to Trash")
            visible: !vaultPage.anyTrashed
            height: visible ? implicitHeight : 0
            onTriggered: vaultPage.deleteChosen()
        }
        MenuItem {
            text: qsTr("Restore")
            visible: vaultPage.anyTrashed
            height: visible ? implicitHeight : 0
            onTriggered: vaultPage.askRestore()
        }
        MenuItem {
            text: qsTr("Delete Permanently…")
            visible: vaultPage.anyTrashed
            height: visible ? implicitHeight : 0
            onTriggered: vaultPage.deleteChosen()
        }
    }

    // Asked before several items, or any item into the Trash (D-062).
    DialogWindow {
        id: placeQuestion
        objectName: "placeQuestion"
        memoryName: "placeQuestion"
        property string question: ""
        property var action: null
        title: qsTr("Chrono Vault")
        standardButtons: Dialog.Yes | Dialog.No
        function ask(words, doIt) {
            question = words
            action = doIt
            open()
        }
        ColumnLayout {
            Label {
                objectName: "placeQuestionText"
                text: placeQuestion.question
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                Layout.preferredWidth: Math.round(360 * Appearance.factor)
                Layout.fillWidth: true
            }
        }
        onAccepted: {
            const doIt = action
            action = null
            if (doIt && VaultController.state === VaultController.Unlocked)
                doIt()
        }
        onRejected: action = null
    }
    DialogWindow {
        id: foreverQuestion
        objectName: "foreverQuestion"
        memoryName: "foreverQuestion"
        property var ids: []
        property string what: ""
        title: qsTr("Delete permanently?")
        standardButtons: Dialog.Yes | Dialog.No
        defaultButton: Dialog.No // Enter never deletes for good
        function ask(chosenIds, name) {
            ids = chosenIds
            what = name
            open()
        }
        ColumnLayout {
            Label {
                text: qsTr("%1 will be removed from the vault for good. This cannot be undone.").arg(foreverQuestion.what)
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                Layout.preferredWidth: Math.round(360 * Appearance.factor)
                Layout.fillWidth: true
            }
        }
        onAccepted: {
            const gone = ids
            vaultPage.actOn(gone, function(all) { VaultController.deleteItems(all) })
        }
    }
    // The vault locks: what was asked is no longer asked.
    Connections {
        target: VaultController
        function onLocking() {
            placeQuestion.reject()
            foreverQuestion.reject()
        }
    }

    // A category edited from its right-click menu (D-056); made when first
    // opened, so the window holds no hidden dialog before.
    Loader {
        id: sidebarCategoryEditor
        active: false
        sourceComponent: CategoryEditor {}
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
                Layout.fillWidth: true
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
