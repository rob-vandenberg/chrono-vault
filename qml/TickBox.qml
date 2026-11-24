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

// --- Version ------------------------------------------------------------
// TickBox.qml 0.5.78

// --- Version history ----------------------------------------------------
// v0.5.78: Initial release (D-061)

// A tick box whose empty box is seen on any background, a list's included:
// the style's box with an outline of its own, as Chrono Mail draws it.
CheckBox {
    id: tickBox

    Rectangle {
        parent: tickBox.indicator
        anchors.fill: parent
        color: "transparent"
        radius: Math.round(2 * Appearance.factor)
        border.width: Math.max(1, Math.round(Appearance.factor))
        border.color: Qt.rgba(tickBox.palette.windowText.r, tickBox.palette.windowText.g,
                              tickBox.palette.windowText.b, 0.55)
    }
}
