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
// PictureButton.qml 0.6.100

// --- Version history ----------------------------------------------------
// v0.6.100: Initial release (D-071)

// A button that is only a picture or a sign: no frame, a gray circle under the
// mouse, a darker one when pressed or while its menu is open, with a blue edge
// when the keyboard is on it (the owner).
ToolButton {
    id: pictureButton
    property bool menuOpen: false
    // Its menu's own button: the vertical dots, the same everywhere.
    property bool dots: false
    implicitWidth: 32 * Appearance.factor
    implicitHeight: 32 * Appearance.factor
    padding: 0
    text: dots ? "⋮" : ""
    font.pointSize: Appearance.fontPointSize * (dots ? 1.5 : 1)
    background: Rectangle {
        objectName: "pictureCircle"
        implicitWidth: 32 * Appearance.factor
        implicitHeight: 32 * Appearance.factor
        radius: width / 2
        readonly property real strength: pictureButton.down || pictureButton.menuOpen ? 0.24
                                         : pictureButton.enabled && pictureButton.hovered ? 0.14 : 0
        color: Qt.rgba(pictureButton.palette.windowText.r, pictureButton.palette.windowText.g,
                       pictureButton.palette.windowText.b, strength)
        border.width: pictureButton.visualFocus ? Math.max(2, Math.round(2 * Appearance.factor)) : 0
        border.color: pictureButton.palette.highlight
    }
}
