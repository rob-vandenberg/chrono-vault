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
// AboutWindow.qml 0.0.8

// --- Version history ----------------------------------------------------
// v0.0.8: The licences in a scrollable text area; Check for update at the bottom left (D-021)
// v0.0.7: Text sizes follow the UI scaling (D-023)
// v0.0.5: Initial release

// About Chrono Vault (D-021), opened with F1 for now, as in Chrono Viewer.
DialogWindow {
    id: aboutWindow

    signal checkRequested()

    memoryName: "about"
    title: qsTr("About Chrono Vault")
    standardButtons: Dialog.Close

    // The icon with the name, the version and the copyright beside it.
    RowLayout {
        spacing: 16
        Layout.fillWidth: true

        Image {
            source: "qrc:/icons/chrono-vault.svg"
            sourceSize: Qt.size(64, 64)
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
            Layout.alignment: Qt.AlignTop
        }
        ColumnLayout {
            spacing: 8
            Layout.fillWidth: true
            Label {
                text: qsTr("Chrono Vault %1").arg(VaultController.version)
                font.bold: true
                font.pointSize: Appearance.fontPointSize * 1.4
            }
            Label { text: qsTr("Copyright © 2026 Rob Vandenberg") }
        }
    }

    // Every licence Chrono Vault ships, ours first, across the whole width.
    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 480
        Layout.preferredHeight: 260
        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
        TextArea {
            objectName: "licenceText"
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

    leftButtons: Button {
        objectName: "checkForUpdateButton"
        text: qsTr("Check for update")
        onClicked: aboutWindow.checkRequested()
    }
}
