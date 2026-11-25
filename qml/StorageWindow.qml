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
// StorageWindow.qml 0.7.108

// --- Version history ----------------------------------------------------
// v0.7.108: Opening: "Open a vault", your vaults on top, the storage locations in one row (D-077)
// v0.6.97: A long text breaks in a window of normal width (D-061)
// v0.6.92: Exact sizes (D-069)
// v0.5.75: "move": where a vault is moved to (D-060)
// v0.3.63: Local at the top, Cloud below it (the owner)
// v0.3.62: Initial release (D-057)

// Where a vault is kept, as the reference program asks it: a tile per
// storage, a large icon above its name, in two groups (ref, D-057). The
// storages still to come get their tiles when they are built.
DialogWindow {
    id: storageWindow
    objectName: "storageWindow"
    memoryName: "storage2"

    // "new", "open" or "move".
    property string mode: "new"
    // Opening: the vaults Chrono Vault knows on top (the owner, D-077); not
    // where they are listed already, as the start-up page's list.
    property bool withVaults: true
    readonly property bool opening: mode === "open"
    // Known, and not the one open now.
    readonly property var known: VaultController.vaults.filter(function(file) {
        return !(file === VaultController.vaultPath && VaultController.state === VaultController.Unlocked)
    })
    readonly property bool listShown: opening && withVaults && known.length > 0
    property string selected: ""

    signal chosen(string storage) // "webdav" or "device"
    signal vaultChosen(string vaultFile)

    title: opening ? qsTr("Open a vault") : qsTr("Select Storage")
    standardButtons: Dialog.Cancel
    rightButtons: Button {
        objectName: "openChosenButton"
        visible: storageWindow.listShown
        text: qsTr("OK")
        highlighted: true
        enabled: storageWindow.selected !== "" && VaultController.foundVaults.indexOf(storageWindow.selected) >= 0
        onClicked: storageWindow.openSelected()
    }
    // The keyboard in the list from the start: arrows choose, Enter opens (D-077).
    onVisibleChanged: {
        if (!visible)
            return
        selected = ""
        if (listShown)
            Qt.callLater(function() { vaultList.forceActiveFocus() })
    }

    function openSelected() {
        if (selected === "" || VaultController.foundVaults.indexOf(selected) < 0)
            return
        const file = selected
        accept()
        vaultChosen(file)
    }

    component Tile: AbstractButton {
        id: tile
        property string iconSource
        implicitWidth: 132 * Appearance.factor
        implicitHeight: tileColumn.implicitHeight + 20 * Appearance.factor
        hoverEnabled: true
        activeFocusOnTab: true
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        background: Rectangle {
            radius: Math.round(8 * Appearance.factor)
            color: tile.down ? Qt.darker(storageWindow.palette.highlight, 1.2)
                 : tile.hovered || tile.activeFocus ? storageWindow.palette.highlight : "transparent"
            opacity: tile.hovered || tile.activeFocus || tile.down ? 0.35 : 1
        }
        contentItem: Item {}
        ExactColumn {
            id: tileColumn
            anchors.centerIn: parent
            gap: 8 * Appearance.factor
            Image {
                source: tile.iconSource
                sourceSize: Qt.size(Math.round(48 * Appearance.factor), Math.round(48 * Appearance.factor))
                Layout.preferredWidth: Math.round(48 * Appearance.factor)
                Layout.preferredHeight: Math.round(48 * Appearance.factor)
                Layout.alignment: Qt.AlignHCenter
            }
            Label {
                text: tile.text
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    function choose(storage) {
        accept()
        chosen(storage)
    }

    // Opening: your vaults, then the storage locations in one row (the owner's
    // design, D-077). The list takes what room the window has beyond the tiles.
    ExactColumn {
        visible: storageWindow.opening
        gap: 10 * Appearance.factor
        Layout.preferredWidth: 560 * Appearance.factor
        Layout.fillWidth: true
        Layout.fillHeight: true

        Label {
            visible: storageWindow.listShown
            text: qsTr("Your vaults")
            font.pointSize: Appearance.fontPointSize * 1.3
        }
        ListView {
            id: vaultList
            objectName: "knownVaults"
            visible: storageWindow.listShown
            clip: true
            spacing: 6 * Appearance.factor
            model: storageWindow.listShown ? storageWindow.known : []
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            readonly property real rowHeight: 64 * Appearance.factor
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: visible ? rowHeight * Math.min(count, 2) : 0
            Layout.preferredHeight: visible ? Math.min(count, 4) * (rowHeight + spacing) : 0
            Keys.onReturnPressed: storageWindow.openSelected()
            Keys.onEnterPressed: storageWindow.openSelected()
            Keys.onUpPressed: storageWindow.selected = model[Math.max(0, model.indexOf(storageWindow.selected) - 1)] || ""
            Keys.onDownPressed: storageWindow.selected = model[Math.min(count - 1, model.indexOf(storageWindow.selected) + 1)] || ""
            delegate: Rectangle {
                id: knownRow
                required property string modelData
                readonly property var place: VaultController.vaultPlace(modelData)
                readonly property bool found: VaultController.foundVaults.indexOf(modelData) >= 0
                readonly property string name: VaultController.vaultNameOf(modelData)
                objectName: "knownVault_" + name
                width: ListView.view.width - 12 * Appearance.factor
                height: vaultList.rowHeight
                radius: Math.round(6 * Appearance.factor)
                color: storageWindow.selected === modelData ? Qt.alpha(storageWindow.palette.highlight, 0.45)
                                                            : storageWindow.palette.base
                border.color: storageWindow.palette.mid
                TapHandler {
                    enabled: knownRow.found
                    onTapped: {
                        storageWindow.selected = knownRow.modelData
                        vaultList.forceActiveFocus()
                    }
                    onDoubleTapped: {
                        storageWindow.selected = knownRow.modelData
                        storageWindow.openSelected()
                    }
                }
                ExactColumn {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 10 * Appearance.factor
                    gap: 2 * Appearance.factor
                    ExactRow {
                        gap: 6 * Appearance.factor
                        Layout.fillWidth: true
                        Label {
                            text: knownRow.name
                            textFormat: Text.PlainText
                            font.bold: true
                            font.strikeout: !knownRow.found
                            elide: Text.ElideRight
                            Layout.maximumWidth: implicitWidth
                            Layout.fillWidth: true
                        }
                        Image {
                            visible: knownRow.modelData === VaultController.primaryVault
                            source: "qrc:/icons/crown.svg"
                            sourceSize: Qt.size(Math.round(18 * Appearance.factor), Math.round(18 * Appearance.factor))
                            Layout.preferredWidth: Math.round(18 * Appearance.factor)
                            Layout.preferredHeight: Math.round(18 * Appearance.factor)
                        }
                        Padlock {
                            visible: knownRow.found
                            vaultFile: knownRow.modelData
                            paper: storageWindow.palette.base
                            ink: storageWindow.palette.windowText
                            Layout.preferredWidth: Math.round(wide)
                            Layout.preferredHeight: Math.round(18 * Appearance.factor)
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            visible: !knownRow.found
                            text: qsTr("Not found")
                            color: "#e53e3e"
                        }
                    }
                    Label {
                        text: knownRow.place.onWebDav
                              ? qsTr("WebDAV: %1").arg(knownRow.place.address)
                                + (knownRow.place.lastSynced ? "   " + qsTr("Last synchronized: %1").arg(knownRow.place.lastSynced) : "")
                              : qsTr("This computer: %1").arg(knownRow.place.address)
                        textFormat: Text.PlainText
                        opacity: 0.7
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }
            }
        }
        Label {
            text: qsTr("Storage locations")
            font.pointSize: Appearance.fontPointSize * 1.3
            Layout.topMargin: storageWindow.listShown ? 8 * Appearance.factor : 0
        }
        ExactRow {
            gap: 8 * Appearance.factor
            Tile {
                objectName: "openDeviceTile"
                text: qsTr("This Device")
                iconSource: "qrc:/icons/storage-device.svg"
                onClicked: storageWindow.choose("device")
            }
            Tile {
                objectName: "openWebDavTile"
                text: qsTr("WebDAV")
                iconSource: "qrc:/icons/storage-webdav.svg"
                onClicked: storageWindow.choose("webdav")
            }
        }
    }

    ExactColumn {
        visible: !storageWindow.opening
        gap: 10 * Appearance.factor
        Layout.preferredWidth: 420 * Appearance.factor

        Label {
            text: storageWindow.mode === "move" ? qsTr("Select where the vault should move to:")
                                                : qsTr("Select where you already have or want to create your vault:")
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("Local")
            font.bold: true
        }
        ExactRow {
            gap: 8 * Appearance.factor
            Tile {
                objectName: "deviceTile"
                text: qsTr("This Device")
                iconSource: "qrc:/icons/storage-device.svg"
                onClicked: storageWindow.choose("device")
            }
        }
        Label {
            text: qsTr("Cloud")
            font.bold: true
        }
        ExactRow {
            gap: 8 * Appearance.factor
            Tile {
                objectName: "webDavTile"
                text: qsTr("WebDAV")
                iconSource: "qrc:/icons/storage-webdav.svg"
                onClicked: storageWindow.choose("webdav")
            }
        }
    }
}
