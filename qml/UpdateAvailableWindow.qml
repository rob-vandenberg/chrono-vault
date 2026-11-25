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
// UpdateAvailableWindow.qml 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Exact sizes (D-069)
// v0.0.9: Initial release, as in Chrono Mail (D-024)

// A newer Chrono Vault: what it is, its page on GitHub, and Download, Skip
// this update or Cancel. The download shows its progress here; Cancel stops it.
DialogWindow {
    id: availableWindow
    objectName: "updateAvailableWindow"

    readonly property int state: Updater.state

    memoryName: "updateAvailable"
    title: qsTr("Update available")
    // Found by itself, it never holds up the rest of the program.
    modality: Qt.NonModal

    Label {
        objectName: "headline"
        text: qsTr("Chrono Vault %1 is available. You have %2.").arg(Updater.newVersion).arg(VaultController.version)
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        Layout.preferredWidth: 420 * Appearance.factor
        Layout.fillWidth: true
    }
    Label {
        objectName: "pageLink"
        visible: Updater.releasePage !== ""
        text: availableWindow.link(Updater.releasePage,
                                   qsTr("What is new in Chrono Vault %1 (on GitHub)").arg(Updater.newVersion))
        textFormat: Text.StyledText
        linkColor: availableWindow.palette.link
        onLinkActivated: Qt.openUrlExternally(Updater.releasePage)
        HoverHandler { cursorShape: Qt.PointingHandCursor }
    }
    ProgressBar {
        visible: availableWindow.state === Updater.Downloading
        value: Updater.progress
        Layout.fillWidth: true
    }
    Label {
        objectName: "note"
        visible: text !== ""
        text: Updater.problem
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }

    rightButtons: [
        Button {
            objectName: "downloadButton"
            text: qsTr("Download")
            highlighted: true
            enabled: availableWindow.state === Updater.Available
            onClicked: Updater.download()
        },
        Button {
            objectName: "skipButton"
            text: qsTr("Skip this update")
            enabled: availableWindow.state === Updater.Available
            onClicked: {
                Updater.skipThisVersion()
                availableWindow.accept()
            }
        }
    ]
    standardButtons: Dialog.Cancel
    // Cancel, Esc or the close button: nothing until the next check; a
    // download under way stops.
    onRejected: Updater.stopDownload()
}
