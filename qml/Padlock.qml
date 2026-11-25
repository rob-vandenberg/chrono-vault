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
// Padlock.qml 0.6.103

// --- Version history ----------------------------------------------------
// v0.6.103: Initial release (D-073)

// Open or locked, seen by shape and color (the owner, D-071).
ColorImage {
    id: padlock
    property bool open: false
    property color paper: "black" // what it is drawn on
    property color ink: "white"
    readonly property real wide: (open ? 22.5 : 18) * Appearance.factor
    source: open ? "qrc:/icons/lock-open.svg" : "qrc:/icons/lock.svg"
    color: open ? (paper.hslLightness < 0.5 ? "#66bb6a" : "#2e7d32") : Qt.tint(paper, Qt.alpha(ink, 0.6))
    sourceSize: Qt.size(Math.round(wide), Math.round(18 * Appearance.factor))
    HoverHandler { id: overPadlock }
    ToolTip.visible: overPadlock.hovered
    ToolTip.text: open ? qsTr("Open") : qsTr("Locked")
}
