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
// UpdateDownloadedWindow.qml 0.0.9

// --- Version history ----------------------------------------------------
// v0.0.9: Initial release, as in Chrono Mail (D-024)

// The update is complete and checked: Install now, or Close and install it by
// hand from the folder shown.
DialogWindow {
    id: downloadedWindow
    objectName: "updateDownloadedWindow"

    readonly property int state: Updater.state
    readonly property bool installing: state === Updater.Installing

    memoryName: "updateDownloaded"
    title: qsTr("Update downloaded")
    // The system's installer is at work: the window waits for its answer.
    closable: !installing

    Label {
        text: qsTr("The update to Chrono Vault %1 was downloaded and checked: it is complete and unchanged.")
              .arg(Updater.newVersion)
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        Layout.preferredWidth: 420
        Layout.fillWidth: true
    }
    Label {
        objectName: "folderLink"
        visible: Updater.downloadFolder.toString() !== ""
        text: downloadedWindow.link(Updater.downloadFolder, qsTr("Saved in %1").arg(Updater.downloadFolderName))
        textFormat: Text.StyledText
        linkColor: downloadedWindow.palette.link
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        onLinkActivated: Qt.openUrlExternally(Updater.downloadFolder)
        HoverHandler { cursorShape: Qt.PointingHandCursor }
    }
    Label {
        objectName: "note"
        visible: text !== ""
        text: downloadedWindow.installing ? Updater.message : Updater.problem
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }

    rightButtons: Button {
        objectName: "installButton"
        text: qsTr("Install")
        highlighted: true
        enabled: downloadedWindow.state === Updater.Downloaded
        onClicked: Updater.install()
    }
    standardButtons: Dialog.Close
    Component.onCompleted: standardButton(Dialog.Close).enabled = Qt.binding(function() { return !downloadedWindow.installing })
}
