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

// --- Version ------------------------------------------------------------
// Padlock.qml 0.6.105

// --- Version history ----------------------------------------------------
// v0.6.105: Another primary vault; Remember my credentials; Open, Closed, Locked (D-074)
// v0.6.103: Initial release (D-073)

// Open or locked, seen by shape and color (the owner, D-071).
ColorImage {
    id: padlock
    property string vaultFile: ""
    // 2 Open, 1 Closed, 0 Locked (the owner, D-074).
    readonly property int vaultState: VaultController.keptRevision >= 0 && VaultController.vaults.length >= 0
                                      && (VaultController.vaultPath !== vaultFile || VaultController.state >= 0)
                                      ? VaultController.vaultStateOf(vaultFile) : 0
    readonly property bool open: vaultState === 2
    property color paper: "black" // what it is drawn on
    property color ink: "white"
    readonly property real wide: (open ? 22.5 : 18) * Appearance.factor
    source: open ? "qrc:/icons/lock-open.svg" : vaultState === 1 ? "qrc:/icons/lock-closed.svg" : "qrc:/icons/lock-locked.svg"
    color: open ? (paper.hslLightness < 0.5 ? "#66bb6a" : "#2e7d32") : Qt.tint(paper, Qt.alpha(ink, 0.6))
    sourceSize: Qt.size(Math.round(wide), Math.round(18 * Appearance.factor))
    HoverHandler { id: overPadlock }
    ToolTip.visible: overPadlock.hovered
    ToolTip.text: open ? qsTr("Open") : vaultState === 1 ? qsTr("Closed") : qsTr("Locked")
}
