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
// StorageWindow.qml 0.5.75

// --- Version history ----------------------------------------------------
// v0.5.75: "move": where a vault is moved to (D-060)
// v0.3.63: Local at the top, Cloud below it (the owner)
// v0.3.62: Initial release (D-057)

// Where a vault is kept, as the reference program asks it: a tile per
// storage, a large icon above its name, in two groups (ref, D-057). The
// storages still to come get their tiles when they are built.
DialogWindow {
    id: storageWindow
    objectName: "storageWindow"
    memoryName: "storage"

    // "new", "open" or "move".
    property string mode: "new"

    signal chosen(string storage) // "webdav" or "device"

    title: qsTr("Select Storage")
    standardButtons: Dialog.Cancel

    component Tile: AbstractButton {
        id: tile
        property string iconSource
        implicitWidth: Math.round(132 * Appearance.factor)
        implicitHeight: tileColumn.implicitHeight + Math.round(20 * Appearance.factor)
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
        ColumnLayout {
            id: tileColumn
            anchors.centerIn: parent
            spacing: Math.round(8 * Appearance.factor)
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

    ColumnLayout {
        spacing: Math.round(10 * Appearance.factor)
        width: Math.round(420 * Appearance.factor)

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
        RowLayout {
            spacing: Math.round(8 * Appearance.factor)
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
        RowLayout {
            spacing: Math.round(8 * Appearance.factor)
            Tile {
                objectName: "webDavTile"
                text: qsTr("WebDAV")
                iconSource: "qrc:/icons/storage-webdav.svg"
                onClicked: storageWindow.choose("webdav")
            }
        }
    }
}
