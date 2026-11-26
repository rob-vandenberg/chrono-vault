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
// VaultPage.qml 0.8.115

// --- Version history ----------------------------------------------------
// v0.8.115: The storage's name under the vault's name (D-080)
// v0.7.111: Question dialogs fit their text (D-079)
// v0.6.100: Padlocks and Open in Settings > Vaults; round picture buttons (D-071)
// v0.6.92: Exact sizes (D-069)
// v0.6.88: Settings > Categories and Settings > Tags (D-066)
// v0.6.86: The audit: its group in the sidebar, its lists, an item left out of it (D-065)
// v0.5.83: The focused item apart from the selected ones; gray marks with a blue bar; Change Category; the empty
//          list's graphic; the vault's counts at the right (D-063)
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
        unlisted = false
        rangeEndId = ""
        selectedIds = chosenItemId !== "" ? [chosenItemId] : []
    }
    // The focused item is chosenItemId, the one shown at the right; the
    // selected ones are what an action applies to. Choosing an item by a
    // click or a plain key selects just that one; Ctrl, Shift and Ctrl+A
    // change the selection and leave the focus where it is (D-063).
    property var selectedIds: []
    // Where a range from the focused item ends; the focused item when empty.
    property string rangeEndId: ""
    property string leftOutId: ""
    // The focused item left the list, as a star taken off under Favorites:
    // not selected until it is listed again.
    property bool unlisted: false
    property string lastFilter: ""
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
                    || (!VaultController.showAudit && filter.startsWith("audit:"))
                    || (!VaultController.showTags && filter.startsWith("tag:"))
                    || (!VaultController.showOthers && (filter === "archive" || filter === "trash")))
                vaultPage.choose("")
        }
    }

    // The sidebar collapsed to its icons (D-042).
    readonly property bool collapsed: VaultController.sidebarCollapsed
    readonly property real collapsedWidth: 64 * Appearance.factor
    property real expandedWidth: 220 * Appearance.factor
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
    readonly property real sidebarMinimum: 180 * Appearance.factor
    // Dragging its splitter: the sidebar holds at its minimum, or at its icons,
    // while the pointer travels 80 pixels more; then it snaps shut or open, as
    // the collapse button does (the owner, 23 November 2026).
    readonly property real snapTravel: 80 * Appearance.factor
    function dragSidebarTo(x) {
        if (!collapsed && x < sidebarMinimum - snapTravel) {
            setCollapsed(true)
            expandedWidth = sidebarMinimum
        } else if (collapsed && x > collapsedWidth + snapTravel) {
            expandedWidth = sidebarMinimum
            setCollapsed(false)
        }
    }

    // A selected line's gray, and the blue bar of the focused one (D-063).
    component ChosenMark: Rectangle {
        id: mark
        property bool selected: false
        property bool focused: false
        property bool hovered: false
        readonly property color ink: vaultPage.palette.text
        color: selected ? Qt.rgba(ink.r, ink.g, ink.b, 0.10)
               : hovered ? Qt.rgba(ink.r, ink.g, ink.b, 0.05) : "transparent"
        radius: Math.round(4 * Appearance.factor)
        Rectangle {
            objectName: "chosenBar"
            visible: mark.focused
            width: 3 * Appearance.factor
            height: parent.height - 12 * Appearance.factor
            anchors.verticalCenter: parent.verticalCenter
            radius: width / 2
            color: vaultPage.palette.highlight
        }
    }

    // An entry of the left column: its icon alone while collapsed, its name
    // then shown on hover (D-042).
    component SideEntry: ItemDelegate {
        display: vaultPage.collapsed ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
        ToolTip.visible: vaultPage.collapsed && hovered
        ToolTip.text: text
        Layout.fillWidth: true
        // The one the list shows: gray, with the blue bar, as a selected and
        // focused item in the list (D-063).
        property bool chosen: false
        background: ChosenMark {
            selected: parent.chosen
            focused: parent.chosen
            hovered: parent.hovered
        }
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
        readonly property int itemCount: counted && VaultController.revision >= 0 && VaultController.audit
                                         ? VaultController.items.countOf(countFilter) : 0
        readonly property bool countShown: counted && VaultController.showItemCounts && !vaultPage.collapsed
                                           && itemCount > 0
        rightPadding: padding + (countShown ? countLabel.implicitWidth + 10 * Appearance.factor : 0)
        Label {
            id: countLabel
            objectName: "count_" + parent.countFilter
            visible: parent.countShown
            text: parent.itemCount
            opacity: 0.6
            anchors.right: parent.right
            anchors.rightMargin: 10 * Appearance.factor
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
    component GroupHeading: ExactRow {
        id: heading
        property string title: ""
        property string key: ""
        property bool folded: false
        signal foldToggled()
        gap: 0
        Layout.topMargin: 14 * Appearance.factor
        Layout.fillWidth: true
        ToolButton {
            objectName: "foldButton_" + heading.key
            // Collapsed: the button spans the column, so its triangle stands
            // centered, as in the owner's mockup.
            Layout.fillWidth: vaultPage.collapsed
            text: heading.folded ? "▸" : "▾"
            opacity: 0.7
            implicitWidth: 24 * Appearance.factor
            implicitHeight: 22 * Appearance.factor
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
    // Settings > Categories and Tags, from the counts (D-066).
    signal categoriesRequested()
    signal tagsRequested()
    // Settings > Security, for the check for leaked passwords (D-065).
    signal leakSettingRequested()
    // The item's sync conflict, to be settled in the Sync conflicts dialog (D-060).
    signal conflictRequested(string itemId)

    // Another of the person's vaults (D-030), after an edit not saved yet is
    // asked about.
    function switchVault(vaultFile) {
        editor.whenFree(function() {
            editor.close()
            chosenItemId = ""
            VaultController.switchToVault(vaultFile)
        })
    }

    function closeEditor() {
        editor.close()
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
            selectedIds = id !== "" ? [id] : []
            rangeEndId = ""
            showRow(VaultController.items.rowOf(id))
        })
    }

    // The list's current row, in view.
    function showRow(row) {
        itemList.currentIndex = row
        if (row >= 0)
            itemList.positionViewAtIndex(row, ListView.Contain)
    }

    // Ctrl+click: selected or no longer; the focus stays (D-063). With no
    // focused item yet, it is a click.
    function toggleItem(id) {
        if (chosenItemId === "") {
            showItem(id)
            return
        }
        rangeEndId = ""
        selectedIds = selectedIds.indexOf(id) >= 0 ? selectedIds.filter(function(one) { return one !== id })
                                                   : selectedIds.concat([id])
    }
    // Shift: everything from the focused item to this row; the focus stays.
    function selectRangeTo(row) {
        const items = VaultController.items
        const from = items.rowOf(chosenItemId)
        if (from < 0) {
            showItem(items.idAt(row))
            return
        }
        const ids = []
        for (let one = Math.min(from, row); one <= Math.max(from, row); ++one)
            ids.push(items.idAt(one))
        selectedIds = ids
        rangeEndId = items.idAt(row)
        // A row not made yet has an estimated place, without the heading
        // above it: made and laid out first, then brought into view.
        itemList.positionViewAtIndex(row, ListView.Contain)
        itemList.forceLayout()
        itemList.positionViewAtIndex(row, ListView.Contain)
    }
    function selectAll() {
        const ids = []
        for (let row = 0; row < itemList.count; ++row)
            ids.push(VaultController.items.idAt(row))
        rangeEndId = ""
        selectedIds = ids
    }
    readonly property bool inTrash: VaultController.filter === "trash"
    // What the chosen items are, for the menus.
    readonly property var chosenItems: VaultController.revision >= 0
                                       ? selectedIds.map(function(id) { return VaultController.item(id) })
                                                    .filter(function(one) { return one.id !== undefined }) : []
    readonly property bool anyTrashed: chosenItems.some(function(one) { return one.trashed === true })
    readonly property bool allArchived: chosenItems.length > 0
                                        && chosenItems.every(function(one) { return one.archived === true })
    readonly property bool allExcluded: chosenItems.length > 0
                                        && chosenItems.every(function(one) { return one.noAudit === true })
    readonly property bool allFavourite: chosenItems.length > 0
                                         && chosenItems.every(function(one) { return one.favourite === true })
    function isTrashed(id) {
        return VaultController.item(id).trashed === true
    }
    // Done to these items; then, when the focused one has left the list, the
    // one after them is focused, so the next key acts on what the person
    // sees. Other items still selected stay selected (D-063).
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
        const before = selectedIds.slice()
        const focused = chosenItemId
        action(ids)
        if (ids.indexOf(focused) >= 0 && items.rowOf(focused) < 0) {
            chosenItemId = ""
            const others = before.filter(function(id) { return items.rowOf(id) >= 0 })
            if (next !== "" && items.rowOf(next) >= 0) {
                showItem(next)
                if (others.length > 0)
                    selectedIds = others
            }
        }
    }
    // What a question is about: one title, or how many.
    function nameOf(ids) {
        return ids.length > 1 ? qsTr("%n item(s)", "", ids.length)
                              : qsTr("“%1”").arg(VaultController.item(ids[0]).title ?? "")
    }
    // An editor open is closed first, asked about when changed.
    function whenClosed(action) {
        editor.whenFree(function() {
            editor.close()
            action()
        })
    }

    // The actions: on the selected items from the menus, on the focused one
    // from its own buttons at the right (D-063).
    function favouriteChosen() {
        const ids = selectedIds.slice()
        const on = !allFavourite
        whenClosed(function() {
            actOn(ids, function(all) { VaultController.placeItems(all, "favourite", on) })
        })
    }
    // Out of the audit, or into it again (D-065).
    function excludeChosen() {
        const ids = selectedIds.slice()
        const on = !allExcluded
        whenClosed(function() {
            actOn(ids, function(all) { VaultController.placeItems(all, "noAudit", on) })
        })
    }
    // Another item, also when the list does not hold it: then with every item.
    function showAnywhere(id) {
        editor.whenFree(function() {
            editor.close()
            VaultController.searchText = ""
            if (VaultController.items.rowOf(id) < 0)
                VaultController.filter = ""
            showItem(id)
        })
    }
    function changeCategory(categoryId) {
        const ids = selectedIds.slice()
        whenClosed(function() {
            actOn(ids, function(all) { VaultController.setCategoryOf(all, categoryId) })
        })
    }
    function restoreItems(ids) {
        const go = function() {
            actOn(ids, function(all) { VaultController.placeItems(all, "trashed", false) })
        }
        if (ids.length > 1)
            placeQuestion.ask(qsTr("Restore %1?").arg(nameOf(ids)), go)
        else
            go()
    }
    // Archive, and Unarchive (D-033).
    function archiveItems(ids) {
        if (ids.length === 0)
            return
        const on = ids.some(function(id) { return VaultController.item(id).archived !== true })
        const go = function() {
            whenClosed(function() {
                actOn(ids, function(all) { VaultController.placeItems(all, "archived", on) })
            })
        }
        if (ids.length > 1)
            placeQuestion.ask(on ? qsTr("Archive %1?").arg(nameOf(ids)) : qsTr("Unarchive %1?").arg(nameOf(ids)), go)
        else
            go()
    }
    // Move to Trash: asked first (D-062); in the Trash, about deleting for
    // good.
    function deleteItems(ids) {
        if (ids.length === 0)
            return
        whenClosed(function() {
            if (ids.some(isTrashed))
                foreverQuestion.ask(ids, nameOf(ids))
            else
                placeQuestion.ask(qsTr("Move %1 to the Trash?").arg(nameOf(ids)), function() {
                    actOn(ids, function(all) { VaultController.placeItems(all, "trashed", true) })
                })
        })
    }
    function restoreChosen() { restoreItems(selectedIds.slice()) }
    function archiveChosen() { archiveItems(selectedIds.slice()) }
    function deleteChosen() { deleteItems(selectedIds.slice()) }

    // Enter or a double click on the item: its editor; not in the Trash.
    function editShown() {
        if (chosenItemId !== "" && !editing && !isTrashed(chosenItemId))
            editChosen()
    }
    function editChosen() {
        if (chosenItemId !== "")
            editor.edit(VaultController.item(chosenItemId), false)
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
            // An item the search left out is shown again with the search gone.
            if (vaultPage.leftOutId !== "" && vaultPage.chosenItemId === "" && editor.itemId === ""
                    && VaultController.items.rowOf(vaultPage.leftOutId) >= 0)
                vaultPage.chosenItemId = vaultPage.leftOutId
            const items = VaultController.items
            const narrowed = VaultController.filter !== vaultPage.lastFilter
            vaultPage.lastFilter = VaultController.filter
            const id = editor.itemId !== "" ? editor.itemId : vaultPage.chosenItemId
            const row = id === "" ? -1 : items.rowOf(id)
            // Only what is in the list is selected (D-063).
            const left = vaultPage.selectedIds.filter(function(one) { return items.rowOf(one) >= 0 })
            // The focused item is no longer shown when it is gone, when the
            // search leaves it out, or when the newly chosen list does not
            // hold it.
            if (editor.itemId === "" && vaultPage.chosenItemId !== "" && row < 0
                    && (narrowed || VaultController.searchText !== "" || !vaultPage.stillShown(vaultPage.chosenItemId))) {
                const out = VaultController.searchText !== "" && vaultPage.stillShown(vaultPage.chosenItemId)
                            ? vaultPage.chosenItemId : ""
                vaultPage.chosenItemId = left.length > 0 ? left[0] : ""
                vaultPage.selectedIds = left
                vaultPage.leftOutId = out
            } else if (left.length !== vaultPage.selectedIds.length) {
                vaultPage.unlisted = vaultPage.chosenItemId !== "" && left.length === 0 && row < 0
                vaultPage.selectedIds = left
            } else if (vaultPage.unlisted && row >= 0) {
                vaultPage.unlisted = false
                vaultPage.selectedIds = [vaultPage.chosenItemId]
            }
            itemList.currentIndex = editor.itemId !== "" ? row : items.rowOf(vaultPage.chosenItemId)
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
            implicitWidth: Math.max(8, 8 * Appearance.factor)
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
            SplitView.preferredWidth: 220 * Appearance.factor
            // Collapsed: a narrow column of fixed width (D-042).
            SplitView.minimumWidth: vaultPage.collapsed ? vaultPage.collapsedWidth : vaultPage.sidebarMinimum
            SplitView.maximumWidth: vaultPage.collapsed ? vaultPage.collapsedWidth : Number.POSITIVE_INFINITY
            padding: vaultPage.collapsed ? 6 * Appearance.factor : 12 * Appearance.factor
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

                ExactColumn {
                    id: sideColumn
                    width: columnScroll.availableWidth
                    gap: 2 * Appearance.factor

                    // Collapses the sidebar to its icons, or expands it (D-042).
                    PictureButton {
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
                        Layout.bottomMargin: 14 * Appearance.factor
                        onClicked: vaultMenu.popup(this, 0, height)
                        ToolTip.visible: vaultPage.collapsed && hovered
                        ToolTip.text: VaultController.vaultName
                        // Collapsed: the icon with its ▾ below it (D-042).
                        contentItem: GridLayout {
                            columns: vaultPage.collapsed ? 1 : 3
                            columnSpacing: 10 * Appearance.factor
                            rowSpacing: 0
                            Image {
                                Layout.alignment: Qt.AlignHCenter
                                source: "qrc:/icons/chrono-vault.svg"
                                sourceSize: Qt.size(Math.round(36 * Appearance.factor), Math.round(36 * Appearance.factor))
                                Layout.preferredWidth: Math.round(36 * Appearance.factor)
                                Layout.preferredHeight: Math.round(36 * Appearance.factor)
                            }
                            ExactColumn {
                                visible: !vaultPage.collapsed
                                gap: 0
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
                                    text: VaultController.inCloud ? VaultController.storageName : qsTr("Local file")
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
                        chosen: VaultController.filter === ""
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("")
                        counted: true; countFilter: ""
                    }
                    SideEntry {
                        objectName: "favouritesEntry"
                        text: qsTr("Favorites")
                        icon.source: "qrc:/icons/star.svg"
                        icon.color: vaultPage.palette.buttonText
                        chosen: VaultController.filter === "favourites"
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
                            chosen: VaultController.filter === "category:" + modelData.id
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
                        Layout.leftMargin: 8 * Appearance.factor
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
                            leftPadding: (8 + (vaultPage.collapsed ? 0 : 16 * modelData.depth)) * Appearance.factor
                            chosen: VaultController.filter === "tag:" + modelData.tag
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

                    // What the audit found (D-065).
                    GroupHeading {
                        visible: VaultController.showAudit
                        title: qsTr("Audit")
                        key: "audit"
                        folded: VaultController.auditFolded
                        onFoldToggled: VaultController.auditFolded = !VaultController.auditFolded
                    }
                    SideEntry {
                        objectName: "audit_leaked"
                        visible: VaultController.showAudit && !VaultController.auditFolded
                        text: qsTr("Leaked")
                        icon.source: "qrc:/icons/audit-leaked.svg"
                        icon.color: vaultPage.palette.buttonText
                        chosen: VaultController.filter === "audit:leaked"
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("audit:leaked")
                        counted: true; countFilter: "audit:leaked"
                    }
                    SideEntry {
                        objectName: "audit_reused"
                        visible: VaultController.showAudit && !VaultController.auditFolded
                        text: qsTr("Reused")
                        icon.source: "qrc:/icons/audit-reused.svg"
                        icon.color: vaultPage.palette.buttonText
                        chosen: VaultController.filter === "audit:reused"
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("audit:reused")
                        counted: true; countFilter: "audit:reused"
                    }
                    SideEntry {
                        objectName: "audit_weak"
                        visible: VaultController.showAudit && !VaultController.auditFolded
                        text: qsTr("Weak")
                        icon.source: "qrc:/icons/audit-weak.svg"
                        icon.color: vaultPage.palette.buttonText
                        chosen: VaultController.filter === "audit:weak"
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("audit:weak")
                        counted: true; countFilter: "audit:weak"
                    }
                    SideEntry {
                        objectName: "audit_old"
                        visible: VaultController.showAudit && !VaultController.auditFolded
                        text: qsTr("Old")
                        icon.source: "qrc:/icons/audit-old.svg"
                        icon.color: vaultPage.palette.buttonText
                        chosen: VaultController.filter === "audit:old"
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("audit:old")
                        counted: true; countFilter: "audit:old"
                    }
                    SideEntry {
                        objectName: "audit_breached"
                        visible: VaultController.showAudit && !VaultController.auditFolded
                        text: qsTr("Breached websites")
                        icon.source: "qrc:/icons/audit-breached.svg"
                        icon.color: vaultPage.palette.buttonText
                        chosen: VaultController.filter === "audit:breached"
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("audit:breached")
                        counted: true; countFilter: "audit:breached"
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
                        chosen: VaultController.filter === "archive"
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
                        chosen: VaultController.filter === "trash"
                        Layout.fillWidth: true
                        onClicked: vaultPage.choose("trash")
                        counted: true; countFilter: "trash"
                    }
                }
            }
        }

        // Middle: the search, the order, and the items under letter headings.
        ExactColumn {
            SplitView.preferredWidth: 300 * Appearance.factor
            SplitView.minimumWidth: 200 * Appearance.factor
            gap: 4 * Appearance.factor

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
                Layout.topMargin: 8 * Appearance.factor
                Layout.leftMargin: 13 * Appearance.factor
                Layout.rightMargin: 13 * Appearance.factor
            }

            ExactRow {
                gap: 2 * Appearance.factor
                Layout.leftMargin: 13 * Appearance.factor
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
            ExactRow {
                visible: vaultPage.inTrash
                gap: 6 * Appearance.factor
                Layout.leftMargin: 13 * Appearance.factor
                readonly property bool any: VaultController.revision >= 0 && VaultController.items.countOf("trash") > 0
                Button {
                    objectName: "restoreAllButton"
                    text: qsTr("Restore all")
                    enabled: parent.any
                    onClicked: {
                        const ids = VaultController.trashedItems()
                        placeQuestion.ask(qsTr("Restore %1?").arg(qsTr("%n item(s)", "", ids.length)), function() {
                            vaultPage.chosenItemId = ""
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
                Layout.leftMargin: 5 * Appearance.factor
                Layout.rightMargin: 5 * Appearance.factor
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
                    // With Shift, on from where the range ends.
                    const end = shift && vaultPage.rangeEndId !== ""
                                ? VaultController.items.rowOf(vaultPage.rangeEndId) : -1
                    const at = end >= 0 ? end : currentIndex
                    let row = at
                    switch (event.key) {
                    case Qt.Key_Up: row = at < 0 ? 0 : at - 1; break
                    case Qt.Key_Down: row = at + 1; break
                    case Qt.Key_PageUp: row = at - page; break
                    case Qt.Key_PageDown: row = at + page; break
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
                    else if (row !== currentIndex || vaultPage.selectedIds.length !== 1)
                        vaultPage.showItem(VaultController.items.idAt(row))
                }

                section.property: "initial"
                section.criteria: ViewSection.FullString
                section.delegate: Label {
                    required property string section
                    text: section
                    font.bold: true
                    opacity: 0.7
                    leftPadding: 12 * Appearance.factor
                    topPadding: 6 * Appearance.factor
                    bottomPadding: 2 * Appearance.factor
                }

                delegate: ItemDelegate {
                    id: listEntry
                    width: ListView.view.width
                    // Selected: gray; focused: the blue bar (D-063).
                    objectName: "itemRow_" + index
                    focusPolicy: Qt.NoFocus
                    readonly property color textColour: vaultPage.palette.text
                    background: ChosenMark {
                        selected: vaultPage.selectedIds.indexOf(model.itemId) >= 0
                        focused: model.itemId === vaultPage.chosenItemId
                        hovered: listEntry.hovered
                    }
                    contentItem: ExactRow {
                        gap: 10 * Appearance.factor
                        Image {
                            source: model.icon
                            sourceSize: Qt.size(Math.round(36 * Appearance.factor), Math.round(36 * Appearance.factor))
                            Layout.preferredWidth: Math.round(36 * Appearance.factor)
                            Layout.preferredHeight: Math.round(36 * Appearance.factor)
                        }
                        ExactColumn {
                            gap: 2 * Appearance.factor
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
                            implicitWidth: 8 * Appearance.factor
                            implicitHeight: implicitWidth
                            radius: width / 2
                            color: "#d32f2f"
                        }
                    }
                    // A click gives the list the keyboard; Ctrl adds or takes
                    // one away, Shift takes a range (D-062, D-063).
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
                    onDoubleClicked: if (heldKeys === Qt.NoModifier) vaultPage.editShown()
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

                EmptyList {
                    objectName: "emptyList"
                    anchors.centerIn: parent
                    width: parent.width - 24 * Appearance.factor
                    visible: itemList.count === 0
                    categories: vaultColumn.categoryList
                    onSettingsRequested: vaultPage.leakSettingRequested()
                }
            }
        }

        // Right: nothing chosen, the chosen item read-only, or the editor.
        StackLayout {
            SplitView.fillWidth: true
            currentIndex: editor.itemId !== "" ? 2 : (vaultPage.chosenItemId !== "" ? 1 : 0)

            VaultOverview {
                objectName: "vaultOverview"
                onFilterChosen: function(filter) { vaultPage.choose(filter) }
                onCategoriesRequested: vaultPage.categoriesRequested()
                onTagsRequested: vaultPage.tagsRequested()
                onLeakSettingRequested: vaultPage.leakSettingRequested()
            }
            ItemDetails {
                itemId: vaultPage.chosenItemId
                revealAll: vaultPage.revealAll
                onEditRequested: vaultPage.editChosen()
                onTrashRequested: vaultPage.deleteItems([vaultPage.chosenItemId])
                onRestoreRequested: vaultPage.restoreItems([vaultPage.chosenItemId])
                onArchiveRequested: vaultPage.archiveItems([vaultPage.chosenItemId])
                onConflictRequested: function(itemId) { vaultPage.conflictRequested(itemId) }
                onItemRequested: function(itemId) { vaultPage.showAnywhere(itemId) }
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
                    // Stored, but not in the list shown: seen, not selected.
                    if (itemList.currentIndex < 0) {
                        vaultPage.selectedIds = []
                        vaultPage.unlisted = true
                    }
                }
            }
        }
    }

    // An item's right-click menu (D-062).
    Menu {
        id: listMenu
        objectName: "itemListMenu"
        // A submenu's entry has no visible of its own to set.
        onAboutToShow: {
            for (let index = 0; index < count; ++index) {
                const entry = itemAt(index)
                if (entry && entry.subMenu) {
                    entry.visible = !vaultPage.anyTrashed
                    entry.height = entry.visible ? entry.implicitHeight : 0
                }
            }
        }
        MenuItem {
            objectName: "listMenuEdit"
            text: qsTr("Edit")
            visible: !vaultPage.several && !vaultPage.anyTrashed
            height: visible ? implicitHeight : 0
            onTriggered: {
                // The one selected may be another than the focused one.
                if (vaultPage.selectedIds[0] !== vaultPage.chosenItemId)
                    vaultPage.showItem(vaultPage.selectedIds[0])
                vaultPage.editShown()
            }
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
        CategoryMenu {
            objectName: "listMenuCategory"
            onChosen: function(categoryId) { vaultPage.changeCategory(categoryId) }
        }
        MenuItem {
            objectName: "listMenuAudit"
            text: vaultPage.allExcluded ? qsTr("Include in Audit") : qsTr("Exclude from Audit")
            visible: !vaultPage.anyTrashed
            height: visible ? implicitHeight : 0
            onTriggered: vaultPage.excludeChosen()
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
            onTriggered: vaultPage.restoreChosen()
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
        fitsContent: true // only asks: as tall as its text (D-079)
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
        ExactColumn {
            Label {
                objectName: "placeQuestionText"
                text: placeQuestion.question
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                Layout.preferredWidth: 360 * Appearance.factor
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
        fitsContent: true // only asks: as tall as its text (D-079)
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
        ExactColumn {
            Label {
                text: qsTr("%1 will be removed from the vault for good. This cannot be undone.").arg(foreverQuestion.what)
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                Layout.preferredWidth: 360 * Appearance.factor
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

    TagDialog {
        id: tagDialog
        objectName: "tagDialog"
    }
}
