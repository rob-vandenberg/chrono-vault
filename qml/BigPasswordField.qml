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
// BigPasswordField.qml 0.6.103

// --- Version history ----------------------------------------------------
// v0.6.103: Initial release (D-073)

// The large rounded password field of the start-up page and of the standard
// dialog (the owner).
PasswordField {
    id: bigField
    font.pointSize: Appearance.fontPointSize * 1.4
    // The placeholder in the normal font, the password in the monospaced one.
    font.family: text === "" ? Appearance.font.family : "Noto Sans Mono"
    leftPadding: 14 * Appearance.factor
    // Rounded, as the owner's design.
    background: Rectangle {
        radius: Math.round(8 * Appearance.factor)
        color: bigField.palette.base
        border.color: bigField.activeFocus ? bigField.palette.highlight : bigField.palette.mid
    }
}
