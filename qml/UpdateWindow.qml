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
// UpdateWindow.qml 0.0.5

// --- Version history ----------------------------------------------------
// v0.0.5: Initial release

// A newer Chrono Vault (D-021): download it, skip it, install it.
DialogWindow {
    id: updateWindow

    readonly property int state: Updater.state

    memoryName: "update"
    title: qsTr("Update Chrono Vault")

    Label {
        text: Updater.message
        wrapMode: Text.Wrap
        Layout.preferredWidth: 420
        Layout.fillWidth: true
    }
    BusyIndicator {
        visible: updateWindow.state === Updater.Checking || updateWindow.state === Updater.Installing
        running: visible
    }
    ProgressBar {
        visible: updateWindow.state === Updater.Downloading
        value: Updater.progress
        Layout.fillWidth: true
    }
    RowLayout {
        Layout.alignment: Qt.AlignRight
        Button {
            visible: updateWindow.state === Updater.Available
            text: qsTr("Download")
            highlighted: true
            onClicked: Updater.download()
        }
        Button {
            visible: updateWindow.state === Updater.Available
            text: qsTr("Skip this version")
            onClicked: {
                Updater.skipThisVersion()
                updateWindow.reject()
            }
        }
        Button {
            visible: updateWindow.state === Updater.Downloading
            text: qsTr("Stop")
            onClicked: Updater.stopDownload()
        }
        Button {
            visible: updateWindow.state === Updater.Downloaded
            text: qsTr("Install")
            highlighted: true
            onClicked: Updater.install()
        }
        Button {
            visible: updateWindow.state !== Updater.Downloading && updateWindow.state !== Updater.Installing
            text: qsTr("Close")
            onClicked: updateWindow.reject()
        }
    }
}
