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
import QtQuick.Controls.impl
import QtQuick.Layouts

// --- Version ------------------------------------------------------------
// VaultBox.qml 0.7.108

// --- Version history ----------------------------------------------------
// v0.7.108: The primary's crown in the list and the field (D-077)
// v0.6.106: Close and Lock per vault; the primary first; back to the vault after a lock (D-075)
// v0.6.105: Another primary vault; Remember my credentials; Open, Closed, Locked (D-074)
// v0.6.103: Initial release (D-073)

// The large field that shows a vault, with its list: the start-up page's and the
// standard dialog's (the owner, D-058, D-073).
ComboBox {
    id: vaultBox
    // The vaults it lists, the one it shows; a last entry to open another; "last
    // used" at the one shown; padlocks; with false only a name, not a list.
    property var listed: []
    property string shown: ""
    property bool withOther: false
    property bool withLastUsed: false
    property bool withPadlocks: false
    property bool isList: true
    // Instead of a vault's name and picture: a server's.
    property string otherName: ""
    property string otherPicture: "qrc:/icons/places-webdav.svg"
    readonly property string other: "\u0001other"
    model: withOther ? listed.concat([other]) : listed
    currentIndex: listed.indexOf(shown)
    font.pointSize: Appearance.fontPointSize * 1.4
    enabled: isList
    indicator.visible: isList
    function placeOf(vaultFile) {
        const place = VaultController.vaultPlace(vaultFile)
        if (!place.inCloud)
            return qsTr("this computer")
        if (!place.onWebDav)
            return place.storageName
        const host = String(place.address).replace(/^[a-z]+:\/\//i, "").split("/")[0]
        return host.toLowerCase().indexOf("pcloud") >= 0 ? qsTr("pCloud") : host
    }
    function iconOf(vaultFile) {
        return VaultController.vaultPlace(vaultFile).placePicture
    }
    contentItem: ExactRow {
        gap: 8 * Appearance.factor
        Item { Layout.preferredWidth: 6 * Appearance.factor }
        // A places picture, one colour in the text colour (the owner, D-079).
        IconImage {
            source: vaultBox.otherName !== "" ? vaultBox.otherPicture : vaultBox.iconOf(vaultBox.shown)
            color: vaultBox.palette.buttonText
            sourceSize: Qt.size(Math.round(24 * Appearance.factor), Math.round(24 * Appearance.factor))
            Layout.preferredWidth: Math.round(24 * Appearance.factor)
            Layout.preferredHeight: Math.round(24 * Appearance.factor)
        }
        Label {
            text: vaultBox.otherName !== "" ? vaultBox.otherName : VaultController.vaultNameOf(vaultBox.shown)
            opacity: 1
            textFormat: Text.PlainText
            elide: Text.ElideRight
            Layout.maximumWidth: implicitWidth
            Layout.fillWidth: true
        }
        // The primary's crown (the owner, D-077).
        Image {
            objectName: "boxCrown"
            visible: vaultBox.otherName === "" && vaultBox.shown !== "" && vaultBox.shown === VaultController.primaryVault
            source: "qrc:/icons/crown.svg"
            sourceSize: Qt.size(Math.round(18 * Appearance.factor), Math.round(18 * Appearance.factor))
            Layout.preferredWidth: Math.round(18 * Appearance.factor)
            Layout.preferredHeight: Math.round(18 * Appearance.factor)
        }
        Item { Layout.fillWidth: true }
    }
    // Rounded, as the owner's design.
    background: Rectangle {
        radius: Math.round(8 * Appearance.factor)
        color: vaultBox.palette.base
        border.color: vaultBox.activeFocus ? vaultBox.palette.highlight : vaultBox.palette.mid
    }
    // The list opens below the field, as big as it, in a rounded frame
    // (the mockup the owner chose).
    popup: Popup {
        // Below the field, scrolling when the room below is short; in a small
        // window as far up as it needs.
        readonly property real fromTop: vaultBox.Window.window && visible ? vaultBox.mapToItem(null, 0, 0).y : 0
        readonly property real inside: vaultBox.Window.window ? vaultBox.Window.window.height : 0
        readonly property real below: inside - fromTop - vaultBox.height - 12 * Appearance.factor
        readonly property bool fits: below >= Math.min(contentItem.implicitHeight + 2, 180 * Appearance.factor)
        implicitHeight: Math.min(contentItem.implicitHeight + 2, fits ? below : inside - 16 * Appearance.factor)
        y: fits ? vaultBox.height + 4 * Appearance.factor
                : Math.min(vaultBox.height + 4 * Appearance.factor, inside - fromTop - implicitHeight - 8 * Appearance.factor)
        width: vaultBox.width
        padding: 1
        margins: 0
        background: Rectangle {
            radius: Math.round(8 * Appearance.factor)
            color: vaultBox.palette.button // as every pop-up (D-067)
            border.color: vaultBox.palette.mid
        }
        contentItem: ListView {
            clip: true
            boundsBehavior: Flickable.StopAtBounds // no bounce (the owner, D-046)
            ScrollIndicator.vertical: ScrollIndicator {}
            implicitHeight: contentHeight
            model: vaultBox.popup.visible ? vaultBox.delegateModel : null
            currentIndex: vaultBox.highlightedIndex
        }
    }
    delegate: ItemDelegate {
        id: vaultRow
        required property string modelData
        required property int index
        readonly property bool isOther: modelData === vaultBox.other
        readonly property bool missing: !isOther && VaultController.foundVaults.indexOf(modelData) < 0
        objectName: isOther ? "openOrCreateAnother" : "unlockVault_" + VaultController.vaultNameOf(modelData)
        width: vaultBox.width - 2
        height: isOther ? 48 * Appearance.factor : 60 * Appearance.factor
        font: vaultBox.font
        highlighted: vaultBox.highlightedIndex === index && !missing
        enabled: !missing
        background: Rectangle {
            color: vaultRow.highlighted ? vaultBox.palette.highlight : "transparent"
            opacity: vaultRow.highlighted ? 0.5 : 1
            radius: Math.round(6 * Appearance.factor)
            // A line above "Open or create another vault…".
            Rectangle {
                visible: vaultRow.isOther
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: vaultBox.palette.mid
            }
        }
        contentItem: ExactRow {
            gap: 10 * Appearance.factor
            IconImage {
                visible: !vaultRow.isOther
                source: vaultRow.isOther ? "" : vaultBox.iconOf(vaultRow.modelData)
                color: vaultBox.palette.buttonText
                sourceSize: Qt.size(Math.round(24 * Appearance.factor), Math.round(24 * Appearance.factor))
                Layout.preferredWidth: Math.round(24 * Appearance.factor)
                Layout.preferredHeight: Math.round(24 * Appearance.factor)
            }
            Label {
                visible: vaultRow.isOther
                text: "+"
                font: vaultBox.font
                horizontalAlignment: Text.AlignHCenter
                Layout.preferredWidth: 24 * Appearance.factor
            }
            ExactColumn {
                gap: 0
                Layout.fillWidth: true
                ExactRow {
                    gap: 6 * Appearance.factor
                    Layout.fillWidth: true
                Label {
                    text: vaultRow.isOther ? qsTr("Open another vault…") : VaultController.vaultNameOf(vaultRow.modelData)
                    textFormat: Text.PlainText
                    font.pointSize: vaultBox.font.pointSize
                    font.strikeout: vaultRow.missing
                    opacity: vaultRow.missing ? 0.45 : vaultRow.isOther ? 0.8 : 1
                    elide: Text.ElideRight
                    Layout.maximumWidth: implicitWidth
                    Layout.fillWidth: true
                }
                Image {
                    objectName: "rowCrown"
                    visible: !vaultRow.isOther && vaultRow.modelData === VaultController.primaryVault
                    source: "qrc:/icons/crown.svg"
                    sourceSize: Qt.size(Math.round(18 * Appearance.factor), Math.round(18 * Appearance.factor))
                    Layout.preferredWidth: Math.round(18 * Appearance.factor)
                    Layout.preferredHeight: Math.round(18 * Appearance.factor)
                }
                Item { Layout.fillWidth: true }
                }
                // Where it is kept, so two vaults of one name are told apart.
                Label {
                    visible: !vaultRow.isOther
                    text: vaultRow.isOther ? "" : vaultBox.placeOf(vaultRow.modelData)
                    textFormat: Text.PlainText
                    opacity: 0.6
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            Padlock {
                objectName: "listPadlock"
                visible: vaultBox.withPadlocks && !vaultRow.isOther && !vaultRow.missing
                vaultFile: vaultRow.modelData
                paper: vaultBox.palette.button
                ink: vaultBox.palette.buttonText
                Layout.preferredWidth: Math.round(wide)
                Layout.preferredHeight: Math.round(18 * Appearance.factor)
            }
            Label {
                visible: !vaultRow.isOther && (vaultRow.missing || (vaultBox.withLastUsed && vaultRow.modelData === VaultController.lastUsedVault))
                text: vaultRow.missing ? qsTr("not found") : qsTr("last used")
                opacity: 0.7
            }
        }
    }
}
