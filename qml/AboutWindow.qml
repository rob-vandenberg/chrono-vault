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
// AboutWindow.qml 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Exact sizes (D-069)
// v0.0.39: A normal scroll bar with arrows, beside the text (D-049)
// v0.0.13: Its size includes the title bar and frame (the owner)
// v0.0.12: Its size gives way to a screen too small for it (the owner)
// v0.0.11: Opens at 555 by 440, never smaller; the licences fill it (the owner)
// v0.0.9: The answer of Check for update beside it; a newer version opens the offer (D-024); the name, the
//         copyright and the link to GitHub lined up with the icon (D-021)
// v0.0.8: The licences in a scrollable text area; Check for update at the bottom left (D-021)
// v0.0.7: Text sizes follow the UI scaling (D-023)
// v0.0.5: Initial release

// About Chrono Vault (D-021), opened with F1 for now, as in Chrono Viewer.
DialogWindow {
    id: aboutWindow

    signal checkRequested()

    memoryName: "about"
    // The whole window, title bar and frame included, is the size set here
    // (the owner): the frame the desktop draws, known once it is shown, is
    // taken off the inside.
    property size frame: Qt.size(0, 0)
    onFrameSwapped: {
        if (frame.width === 0 && frame.height === 0) {
            const drawn = Appearance.frameOf(this)
            if (drawn.width > 0 || drawn.height > 0)
                frame = drawn
        }
    }
    // Opens at 555 by 440 and is never smaller, also when a smaller size was
    // remembered from before (the owner).
    width: Math.min(Math.round(555 * Appearance.factor), Appearance.screenRoom(this).width) - frame.width
    height: Math.min(Math.round(440 * Appearance.factor), Appearance.screenRoom(this).height) - frame.height
    minimumWidth: Math.min(Math.round(555 * Appearance.factor), Appearance.screenRoom(this).width) - frame.width
    minimumHeight: Math.min(Math.round(440 * Appearance.factor), Appearance.screenRoom(this).height) - frame.height
    title: qsTr("About Chrono Vault")
    standardButtons: Dialog.Close

    // The icon with the name, the version and the copyright beside it.
    ExactRow {
        gap: 16 * Appearance.factor
        Layout.fillWidth: true

        Image {
            source: "qrc:/icons/chrono-vault.svg"
            sourceSize: Qt.size(Math.round(64 * Appearance.factor), Math.round(64 * Appearance.factor))
            Layout.preferredWidth: Math.round(64 * Appearance.factor)
            Layout.preferredHeight: Math.round(64 * Appearance.factor)
            Layout.alignment: Qt.AlignTop
        }
        // The name at the top of the icon, the link at its bottom, the
        // copyright exactly in the middle between them.
        Item {
            id: heading
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(64 * Appearance.factor, nameLine.height + copyrightLine.height + siteLine.height)
            implicitWidth: Math.max(nameLine.implicitWidth, copyrightLine.implicitWidth, siteLine.implicitWidth)
            Layout.alignment: Qt.AlignTop

            Label {
                id: nameLine
                objectName: "nameLine"
                anchors.top: parent.top
                text: qsTr("Chrono Vault %1").arg(VaultController.version)
                font.bold: true
                font.pointSize: Appearance.fontPointSize * 1.4
                topPadding: 0
                bottomPadding: 0
            }
            Label {
                id: siteLine
                objectName: "siteLine"
                readonly property string address: "https://github.com/rob-vandenberg/chrono-vault"
                anchors.bottom: parent.bottom
                text: aboutWindow.link(address, address)
                textFormat: Text.StyledText
                linkColor: aboutWindow.palette.link
                topPadding: 0
                bottomPadding: 0
                onLinkActivated: Qt.openUrlExternally(address)
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
            Label {
                id: copyrightLine
                objectName: "copyrightLine"
                y: (nameLine.y + nameLine.height / 2 + siteLine.y + siteLine.height / 2) / 2 - height / 2
                text: qsTr("Copyright © 2026 Rob Vandenberg")
                topPadding: 0
                bottomPadding: 0
            }
        }
    }

    // Every licence Chrono Vault ships, ours first, filling the window between
    // the top lines and the buttons.
    ScrollView {
        id: licenceScroll
        contentWidth: availableWidth
        contentHeight: licenceText.implicitHeight
        Layout.fillWidth: true
        Layout.fillHeight: true
        // More room around it (the owner): 8 pixels above, 2 below.
        Layout.topMargin: 8 * Appearance.factor
        Layout.bottomMargin: 2 * Appearance.factor
        // A normal scroll bar, arrows at both ends, always there (the owner,
        // D-046, D-049): the style's own, beside the text.
        ScrollBar.vertical: ScrollBar {
            id: licenceScrollBar
            objectName: "licenceScrollBar"
            parent: licenceScroll
            x: licenceScroll.mirrored ? 0 : licenceScroll.width - width
            y: licenceScroll.topPadding
            height: licenceScroll.height - licenceScroll.topPadding
            orientation: Qt.Vertical
            policy: ScrollBar.AlwaysOn
        }
        rightPadding: licenceScrollBar.width + 6 * Appearance.factor
        // Up/Down a line, Page Up/Down a page, Home/End the start and end,
        // as a person expects of a text to read (the owner, D-046).
        function scrollTo(y) {
            const flick = contentItem
            flick.contentY = Math.max(0, Math.min(y, flick.contentHeight - flick.height))
        }
        function scrollBy(step) {
            scrollTo(contentItem.contentY + step)
        }
        TextArea {
            id: licenceText
            width: licenceScroll.availableWidth
            objectName: "licenceText"
            focus: true
            Keys.onPressed: function(event) {
                const line = Math.round((font.pixelSize > 0 ? font.pixelSize * 1.5 : 20) * Appearance.factor)
                const pageStep = licenceScroll.height - line
                switch (event.key) {
                case Qt.Key_Up: licenceScroll.scrollBy(-line); break
                case Qt.Key_Down: licenceScroll.scrollBy(line); break
                case Qt.Key_PageUp: licenceScroll.scrollBy(-pageStep); break
                case Qt.Key_PageDown: licenceScroll.scrollBy(pageStep); break
                case Qt.Key_Home: licenceScroll.scrollTo(0); break
                case Qt.Key_End: licenceScroll.scrollTo(licenceScroll.contentItem.contentHeight); break
                default: return
                }
                event.accepted = true
            }
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            textFormat: TextEdit.RichText
            text: Licences.text
            background: Rectangle {
                color: aboutWindow.palette.base
                border.color: aboutWindow.palette.mid
            }
        }
    }

    leftButtons: [
        Button {
            objectName: "checkForUpdateButton"
            text: qsTr("Check for update")
            enabled: Updater.state !== Updater.Checking
            onClicked: {
                aboutWindow.asked = true
                aboutWindow.checkRequested()
            }
        },
        // The answer of a check asked for here: looking, the newest already,
        // GitHub not reached, or not installed with an installer. A newer
        // version closes this window and opens the offer.
        Label {
            objectName: "updateNote"
            visible: aboutWindow.asked && text !== ""
            text: Updater.state === Updater.Checking || Updater.state === Updater.Current
                  || Updater.state === Updater.Unreachable || Updater.state === Updater.Failed ? Updater.message : ""
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            Layout.maximumWidth: 260 * Appearance.factor
        }
    ]
    // Help > Check for update.
    // The text takes the keyboard as the box opens, so the keys scroll at once.
    onVisibleChanged: if (visible) licenceText.forceActiveFocus()

    function checkNow() {
        asked = true
        checkRequested()
    }
    // Only the answer to a check asked for in this window is shown here.
    property bool asked: false
    onShown: asked = false
}
