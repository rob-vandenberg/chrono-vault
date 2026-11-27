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
// PasswordField.qml 0.8.120

// --- Version history ----------------------------------------------------
// v0.8.120: The placeholder in the normal font, as every password field (D-082)
// v0.6.104: The eye over the typed text, so it can be clicked
// v0.6.92: Exact sizes (D-069)
// v0.4.70: eyeAllowed: the eye only where it may be shown (D-059)
// v0.0.4: Initial release

// A field for a password, with an eye inside it that shows or hides what
// was typed. Used wherever a password is typed or shown.
TextField {
    id: passwordField

    property bool revealed: false
    // Where the eye may be shown at all (the PIN: only while one is typed).
    property bool eyeAllowed: true

    echoMode: revealed ? TextInput.Normal : TextInput.Password
    // The placeholder in the normal font, the password in the monospaced
    // one, in every password field (the owner, D-082).
    font.family: text === "" ? Appearance.font.family : "Noto Sans Mono"
    rightPadding: (eyeButton.visible ? eyeButton.width : 0) + 6 * Appearance.factor

    ToolButton {
        id: eyeButton
        objectName: "passwordEye"
        parent: passwordField
        z: 1 // over the typed text, which fills the whole field
        visible: passwordField.eyeAllowed
        anchors.right: parent.right
        anchors.rightMargin: 2 * Appearance.factor
        anchors.verticalCenter: parent.verticalCenter
        height: parent.height - 4 * Appearance.factor
        width: height
        focusPolicy: Qt.NoFocus
        icon.source: passwordField.revealed ? "qrc:/icons/eye-hidden.svg" : "qrc:/icons/eye.svg"
        icon.width: Math.round(18 * Appearance.factor)
        icon.height: Math.round(18 * Appearance.factor)
        icon.color: passwordField.palette.text
        ToolTip.visible: hovered
        ToolTip.text: passwordField.revealed ? qsTr("Hide the password") : qsTr("Show the password")
        onClicked: passwordField.revealed = !passwordField.revealed
    }
}
