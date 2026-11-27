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
import QtQuick.Dialogs
import QtQuick.Layouts

// --- Version ------------------------------------------------------------
// IconPicker.qml 0.8.120

// --- Version history ----------------------------------------------------
// v0.8.120: Resized by the person; the question in the one width (D-082)
// v0.7.111: Question dialogs fit their text (D-079)
// v0.6.94: The main window keeps its size, the others scale along (D-070)
// v0.6.92: Exact sizes (D-069)
// v0.6.91: Opens in Search; the icons by keyboard (D-068)
// v0.5.82: Enter never deletes an icon (D-062)
// v0.2.57: A search field: by name, any case, anywhere in the name (the owner, D-050)
// v0.2.51: Its file dialogs as Chrono Viewer's (D-055)
// v0.0.40: Initial release (D-050)

// "Choose Icon" (D-050), after the reference program's picker: the vault's
// own icons first, with the category's icon as "Default icon", then one
// section per folder that icons.md names in the person's icon folder. The
// chosen icon carries a blue check; OK is greyed until one is chosen. A
// right-click on one of the vault's own offers "Delete icon".
DialogWindow {
    id: picker
    objectName: "iconPicker"
    title: qsTr("Choose Icon")
    memoryName: "iconPicker"
    // Resized by the person, who finds it at that size again (D-082).
    fitsContent: false
    firstWidth: 480 * Appearance.factor
    firstHeight: 680 * Appearance.factor
    standardButtons: Dialog.Ok | Dialog.Cancel

    // The chosen icon: "" none yet, "default" the category's own, "custom:"
    // and the PNG in base64, or "file:" and a shipped icon's address.
    property string chosen: ""
    property url defaultSource
    property var customIcons: []
    property var sections: []
    // What is typed in the search field, and the groups that match it: by
    // the icon's name as shown, any case, anywhere in the name; a group with
    // no match is left out (the owner).
    readonly property string searchText: searchField.text.trim().toLowerCase()
    readonly property var shownSections: searchText === "" ? sections : sections.map(function(section) {
        return { heading: section.heading,
                 icons: section.icons.filter(function(icon) {
                     return icon.name.toLowerCase().indexOf(picker.searchText) >= 0 }) }
    }).filter(function(section) { return section.icons.length > 0 })
    // The icon a right-click was made on.
    property string menuIcon: ""
    readonly property int tileSize: Math.round(44 * Appearance.factor)

    // The icon to keep: PNG in base64, or "" for the category's own.
    signal iconChosen(string icon)
    // One of the vault's own icons was deleted: an item being edited that
    // carries it gets its category's icon too.
    signal iconDeleted(string icon)

    onShown: searchField.forceActiveFocus(Qt.TabFocusReason)
    onSearchTextChanged: grid.outlined = null

    function start(categoryIcon) {
        defaultSource = categoryIcon
        chosen = ""
        grid.outlined = null
        searchField.text = ""
        reload()
        open()
    }
    function reload() {
        customIcons = VaultController.customIcons()
        sections = VaultController.iconSections()
    }
    Component.onCompleted: {
        standardButton(Dialog.Ok).objectName = "okButton"
        standardButton(Dialog.Ok).enabled = Qt.binding(function() { return picker.chosen !== "" })
    }
    onAccepted: {
        if (chosen === "default") {
            iconChosen("")
        } else if (chosen.startsWith("custom:")) {
            iconChosen(chosen.substring(7))
        } else if (chosen.startsWith("file:")) {
            // A shipped icon is read into the item as its own PNG, so the
            // vault shows it on every device.
            const read = VaultController.readItemIcon(chosen.substring(5))
            if (read !== "")
                iconChosen(read)
        }
    }

    // One round icon; the chosen one with a blue check.
    component Tile: Item {
        id: tile
        property string key
        property url source
        property string tip
        property string customIcon: ""
        readonly property bool iconTile: true
        // Where the keys are; the blue check is what OK takes.
        Rectangle {
            objectName: "tileOutline"
            visible: grid.activeFocus && grid.outlined === tile
            anchors.fill: parent
            anchors.margins: -3 * Appearance.factor
            color: "transparent"
            radius: Math.round(6 * Appearance.factor)
            border.width: Math.max(2, Math.round(2 * Appearance.factor))
            border.color: picker.palette.highlight
        }
        width: picker.tileSize
        height: picker.tileSize
        Image {
            anchors.fill: parent
            anchors.margins: Math.round(2 * Appearance.factor)
            source: tile.source
            sourceSize: Qt.size(width * 2, height * 2)
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            smooth: true
        }
        Rectangle {
            objectName: "checkMark"
            visible: picker.chosen === tile.key
            width: 18 * Appearance.factor
            height: width
            radius: width / 2
            color: "#2f80ed"
            border.color: "white"
            border.width: Math.max(1, Math.round(1.5 * Appearance.factor))
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: -2 * Appearance.factor
            Label {
                anchors.centerIn: parent
                text: "✓"
                color: "white"
                font.bold: true
                font.pixelSize: 12
            }
        }
        MouseArea {
            id: tileArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: function(mouse) {
                if (mouse.button === Qt.RightButton) {
                    if (tile.customIcon !== "") {
                        picker.menuIcon = tile.customIcon
                        iconMenu.popup()
                    }
                } else {
                    picker.chosen = tile.key
                }
            }
            onDoubleClicked: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    picker.chosen = tile.key
                    picker.accept()
                }
            }
        }
        ToolTip.visible: tileArea.containsMouse && tip !== ""
        ToolTip.text: tip
    }

    component Heading: Label {
        font.bold: true
        opacity: 0.7
        Layout.topMargin: 8 * Appearance.factor
    }

    // As the search field above the item list (the owner).
    TextField {
        id: searchField
        objectName: "iconSearchField"
        placeholderText: qsTr("Search")
        Layout.fillWidth: true
    }

    ScrollView {
        id: scroll
        objectName: "iconScroll"
        Layout.fillWidth: true
        Layout.fillHeight: true
        implicitHeight: 200 * Appearance.factor
        contentWidth: availableWidth
        clip: true
        rightPadding: scrollBar.width + 6 * Appearance.factor
        ScrollBar.vertical: ScrollBar {
            id: scrollBar
            parent: scroll
            x: scroll.mirrored ? 0 : scroll.width - width
            y: scroll.topPadding
            height: scroll.height - scroll.topPadding
            orientation: Qt.Vertical
        }

        // The icons by keyboard (the owner, D-068): Tab comes here from Search
        // and outlines the first one; the keys move the outline, over the
        // headings; Enter takes that icon.
        ExactColumn {
            id: grid
            objectName: "iconGrid"
            activeFocusOnTab: true
            property Item outlined: null
            onActiveFocusChanged: if (activeFocus && !outlined) outline(tiles()[0])
            function tiles() {
                const found = []
                const walk = function(item) {
                    for (const child of item.children) {
                        if (!child.visible)
                            continue
                        if (child.iconTile === true)
                            found.push(child)
                        else
                            walk(child)
                    }
                }
                walk(grid)
                return found
            }
            function placeOf(tile) {
                return tile.mapToItem(grid, 0, 0)
            }
            function outline(tile) {
                outlined = tile || null
                if (!outlined)
                    return
                // In view, the first row with its heading.
                const view = scroll.contentItem
                const margin = 6 * Appearance.factor
                const top = placeOf(outlined).y - margin
                const bottom = top + outlined.height + 2 * margin
                if (outlined === tiles()[0])
                    view.contentY = 0
                else if (top < view.contentY)
                    view.contentY = top
                else if (bottom > view.contentY + view.height)
                    view.contentY = bottom - view.height
            }
            // The icon nearest below or above, at least this far away.
            function beside(tile, down, least) {
                const from = placeOf(tile)
                let best = null
                let bestAt = null
                for (const one of tiles()) {
                    const at = placeOf(one)
                    const away = down ? at.y - from.y : from.y - at.y
                    if (away < least)
                        continue
                    const bestAway = bestAt ? (down ? bestAt.y - from.y : from.y - bestAt.y) : Infinity
                    if (away < bestAway - 1
                            || (Math.abs(away - bestAway) <= 1 && Math.abs(at.x - from.x) < Math.abs(bestAt.x - from.x))) {
                        best = one
                        bestAt = at
                    }
                }
                return best
            }
            Keys.onPressed: function(event) {
                if ((event.modifiers & ~Qt.KeypadModifier) !== Qt.NoModifier)
                    return
                const all = tiles()
                const at = all.indexOf(outlined)
                if (all.length === 0)
                    return
                // The outlined one is gone (an icon deleted): the first again.
                if (at < 0) {
                    if ([Qt.Key_Left, Qt.Key_Right, Qt.Key_Up, Qt.Key_Down, Qt.Key_PageUp, Qt.Key_PageDown, Qt.Key_Home,
                         Qt.Key_End, Qt.Key_Return, Qt.Key_Enter].indexOf(event.key) >= 0) {
                        event.accepted = true
                        outline(all[0])
                    }
                    return
                }
                const page = Math.max(1, scroll.contentItem.height - 2 * outlined.height)
                let to = null
                switch (event.key) {
                case Qt.Key_Left: to = all[Math.max(0, at - 1)]; break
                case Qt.Key_Right: to = all[Math.min(all.length - 1, at + 1)]; break
                case Qt.Key_Up: to = beside(outlined, false, 1); break
                case Qt.Key_Down: to = beside(outlined, true, 1); break
                case Qt.Key_PageUp: to = beside(outlined, false, page) || all[0]; break
                case Qt.Key_PageDown: to = beside(outlined, true, page) || all[all.length - 1]; break
                case Qt.Key_Home: to = all[0]; break
                case Qt.Key_End: to = all[all.length - 1]; break
                case Qt.Key_Return:
                case Qt.Key_Enter:
                    event.accepted = true
                    picker.chosen = outlined.key
                    picker.accept()
                    return
                default: return
                }
                event.accepted = true
                if (to)
                    outline(to)
            }
            // Room for the outline of the first column.
            x: 4 * Appearance.factor
            width: scroll.availableWidth - x
            gap: 8 * Appearance.factor

            // The vault's own icons have no names: not shown while searching.
            Heading {
                text: qsTr("Custom Icons")
                visible: picker.searchText === ""
            }
            Flow {
                objectName: "customIcons"
                visible: picker.searchText === ""
                Layout.fillWidth: true
                spacing: 10 * Appearance.factor
                Tile {
                    objectName: "defaultIconTile"
                    key: "default"
                    source: picker.defaultSource
                    tip: qsTr("Default icon")
                }
                Repeater {
                    model: picker.customIcons
                    delegate: Tile {
                        required property string modelData
                        key: "custom:" + modelData
                        source: "data:image/png;base64," + modelData
                        customIcon: modelData
                    }
                }
            }

            Repeater {
                model: picker.shownSections
                delegate: ExactColumn {
                    id: section
                    required property var modelData
                    Layout.fillWidth: true
                    gap: 8 * Appearance.factor
                    Heading { text: section.modelData.heading }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 10 * Appearance.factor
                        Repeater {
                            model: section.modelData.icons
                            delegate: Tile {
                                required property var modelData
                                key: "file:" + modelData.url
                                source: modelData.url
                                tip: modelData.name
                            }
                        }
                    }
                }
            }
        }
    }

    leftButtons: [
        Button {
            objectName: "addCustomIconButton"
            text: qsTr("Add Custom Icon")
            onClicked: addDialog.open()
        }
    ]

    Menu {
        id: iconMenu
        Action {
            text: qsTr("Delete icon")
            onTriggered: {
                const uses = VaultController.customIconUses(picker.menuIcon)
                if (uses > 0) {
                    deleteQuestion.uses = uses
                    deleteQuestion.open()
                } else {
                    picker.deleteIcon(picker.menuIcon)
                }
            }
        }
    }

    function deleteIcon(icon) {
        VaultController.deleteCustomIcon(icon)
        if (chosen === "custom:" + icon)
            chosen = ""
        reload()
        iconDeleted(icon)
    }

    FilePicker {
        id: addDialog
        memoryName: "IconPicker.addDialog"
        parentWindow: picker
        title: qsTr("Add Custom Icon")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Pictures (*.jpg *.jpeg *.png *.bmp *.ico *.svg)")]
        onAccepted: {
            const added = VaultController.addCustomIcon(selectedFile)
            if (added !== "") {
                picker.reload()
                picker.chosen = "custom:" + added
            }
        }
    }

    // A custom icon that items carry: asked first (the owner).
    DialogWindow {
        id: deleteQuestion
        fitsContent: true // only asks: as tall as its text (D-079)
        objectName: "deleteIconQuestion"
        property int uses: 0
        title: qsTr("Delete icon")
        transientParent: picker
        standardButtons: Dialog.Yes | Dialog.No
        defaultButton: Dialog.No
        Label {
            text: qsTr("This icon is used by %n item(s). They will get their category's icon instead. Delete it?", "", deleteQuestion.uses)
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        onAccepted: picker.deleteIcon(picker.menuIcon)
    }
}
