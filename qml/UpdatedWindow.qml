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
// UpdatedWindow.qml 0.0.9

// --- Version history ----------------------------------------------------
// v0.0.9: Initial release, as in Chrono Mail (D-024)

// Linux, once the system has installed the update: Restart or Close. Chrono
// Vault closes either way, its files being the new version's now; only
// Restart starts it again.
DialogWindow {
    id: updatedWindow
    objectName: "updatedWindow"

    memoryName: "updated"
    title: qsTr("Chrono Vault was updated")

    Label {
        text: Updater.message
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        Layout.preferredWidth: Math.round(420 * Appearance.factor)
        Layout.fillWidth: true
    }

    rightButtons: Button {
        objectName: "restartButton"
        text: qsTr("Restart")
        highlighted: true
        onClicked: {
            updatedWindow.accept()
            Updater.finishUpdate(true)
        }
    }
    standardButtons: Dialog.Close
    onRejected: Updater.finishUpdate(false)
}
