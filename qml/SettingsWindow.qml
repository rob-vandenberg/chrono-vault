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
// SettingsWindow.qml 0.0.3

// --- Version history ----------------------------------------------------
// v0.0.3: Initial release

// Every setting in one window: the topics on the left, the chosen topic's
// settings on the right (D-011).
ApplicationWindow {
    id: settingsWindow

    function showWindow() {
        show()
        raise()
        requestActivate()
    }

    title: qsTr("Settings – Chrono Vault")
    width: 640
    height: 400
    visible: false

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ListView {
            id: topics
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            currentIndex: 0
            model: [qsTr("Security"), qsTr("Clipboard")]
            delegate: ItemDelegate {
                required property int index
                required property string modelData
                width: ListView.view.width
                text: modelData
                highlighted: ListView.isCurrentItem
                onClicked: topics.currentIndex = index
            }
        }

        ToolSeparator {
            orientation: Qt.Vertical
            Layout.fillHeight: true
        }

        StackLayout {
            currentIndex: topics.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 16

            ColumnLayout {
                spacing: 8
                Label {
                    text: qsTr("Security")
                    font.pointSize: 14
                }
                Label {
                    text: qsTr("Lock the vault after this many minutes without use (0 means never):")
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                SpinBox {
                    from: 0
                    to: 240
                    value: VaultController.autoLockMinutes
                    editable: true
                    onValueModified: VaultController.autoLockMinutes = value
                }
                Item { Layout.fillHeight: true }
            }

            ColumnLayout {
                spacing: 8
                Label {
                    text: qsTr("Clipboard")
                    font.pointSize: 14
                }
                Label {
                    text: qsTr("Clear what was copied after this many seconds (0 means never):")
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                SpinBox {
                    from: 0
                    to: 600
                    value: VaultController.clipboardClearSeconds
                    editable: true
                    onValueModified: VaultController.clipboardClearSeconds = value
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
