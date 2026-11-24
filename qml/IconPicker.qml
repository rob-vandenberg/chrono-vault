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
// IconPicker.qml 0.5.82

// --- Version history ----------------------------------------------------
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
    width: Math.round(480 * Appearance.factor)
    height: Math.round(680 * Appearance.factor)
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

    function start(categoryIcon) {
        defaultSource = categoryIcon
        chosen = ""
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
            width: Math.round(18 * Appearance.factor)
            height: width
            radius: width / 2
            color: "#2f80ed"
            border.color: "white"
            border.width: Math.max(1, Math.round(1.5 * Appearance.factor))
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: -Math.round(2 * Appearance.factor)
            Label {
                anchors.centerIn: parent
                text: "✓"
                color: "white"
                font.bold: true
                font.pixelSize: Math.round(12 * Appearance.factor)
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
        Layout.topMargin: Math.round(8 * Appearance.factor)
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
        implicitHeight: Math.round(200 * Appearance.factor)
        contentWidth: availableWidth
        clip: true
        rightPadding: scrollBar.width + Math.round(6 * Appearance.factor)
        ScrollBar.vertical: ScrollBar {
            id: scrollBar
            parent: scroll
            x: scroll.mirrored ? 0 : scroll.width - width
            y: scroll.topPadding
            height: scroll.height - scroll.topPadding
            orientation: Qt.Vertical
        }

        ColumnLayout {
            width: scroll.availableWidth
            spacing: Math.round(8 * Appearance.factor)

            // The vault's own icons have no names: not shown while searching.
            Heading {
                text: qsTr("Custom Icons")
                visible: picker.searchText === ""
            }
            Flow {
                objectName: "customIcons"
                visible: picker.searchText === ""
                Layout.fillWidth: true
                spacing: Math.round(10 * Appearance.factor)
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
                delegate: ColumnLayout {
                    id: section
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Math.round(8 * Appearance.factor)
                    Heading { text: section.modelData.heading }
                    Flow {
                        Layout.fillWidth: true
                        spacing: Math.round(10 * Appearance.factor)
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
            Layout.preferredWidth: Math.round(360 * Appearance.factor)
        }
        onAccepted: picker.deleteIcon(picker.menuIcon)
    }
}
