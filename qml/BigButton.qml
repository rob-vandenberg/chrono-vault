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
// BigButton.qml 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Exact sizes (D-069)
// v0.3.66: Initial release (D-058)

// A button with the weight of the start page's fields: as tall, the same
// large text, rounded (the owner, D-058).
Button {
    id: bigButton
    implicitHeight: 58 * Appearance.factor
    font.pointSize: Appearance.fontPointSize * 1.4
    background: Rectangle {
        radius: Math.round(8 * Appearance.factor)
        color: bigButton.down ? Qt.darker(bigButton.palette.button, 1.15)
             : bigButton.hovered ? Qt.lighter(bigButton.palette.button, 1.15) : bigButton.palette.button
        border.color: bigButton.activeFocus ? bigButton.palette.highlight : bigButton.palette.mid
    }
}
