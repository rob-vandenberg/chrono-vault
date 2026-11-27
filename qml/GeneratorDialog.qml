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
// GeneratorDialog.qml 0.8.121

// --- Version history ----------------------------------------------------
// v0.8.121: The design of every dialog: groups 16 apart, the tick boxes close together (D-082)
// v0.8.120: The one width of every dialog (D-082)
// v0.6.92: Exact sizes (D-069)
// v0.5.78: Its content follows the window's width (D-061)
// v0.0.4: The password in Noto Sans Mono
// v0.0.3: Initial release

// Makes a strong password to use in a field.
DialogWindow {
    id: generatorDialog
    memoryName: "generator"

    property string password: ""
    // At least one kind stays ticked, so the password is what the boxes say.
    readonly property int kindsTicked: (uppercase.checked ? 1 : 0) + (lowercase.checked ? 1 : 0)
                                       + (digits.checked ? 1 : 0) + (symbols.checked ? 1 : 0)

    signal usePassword(string password)

    function makeOne() {
        password = VaultController.generatePassword(lengthSlider.value, uppercase.checked, lowercase.checked,
                                                    digits.checked, symbols.checked)
    }

    objectName: "generatorDialog"
    title: qsTr("Generate a password")
    standardButtons: Dialog.Ok | Dialog.Cancel

    onShown: makeOne()
    onAccepted: usePassword(password)

    // The design of every dialog (the owner, D-082): 16 between groups, the
    // tick boxes close together.
    ExactColumn {
        gap: 16 * Appearance.factor

        TextField {
            text: generatorDialog.password
            readOnly: true
            font.family: "Noto Sans Mono"
            selectByMouse: true
            Layout.fillWidth: true
        }
        ExactRow {
            gap: 12 * Appearance.factor
            Layout.fillWidth: true
            Label { text: qsTr("Length: %1").arg(lengthSlider.value) }
            Slider {
                id: lengthSlider
                from: 8
                to: 64
                stepSize: 1
                value: 20
                Layout.fillWidth: true
                onMoved: generatorDialog.makeOne()
            }
        }
        ExactColumn {
            gap: 4 * Appearance.factor
        CheckBox {
            id: uppercase
            // 22 high, at the left edge (D-082).
            topPadding: 2 * Appearance.factor
            bottomPadding: 2 * Appearance.factor
            leftPadding: 0
            text: qsTr("Capital letters (A-Z)")
            checked: true
            enabled: !checked || generatorDialog.kindsTicked > 1
            onToggled: generatorDialog.makeOne()
        }
        CheckBox {
            id: lowercase
            // 22 high, at the left edge (D-082).
            topPadding: 2 * Appearance.factor
            bottomPadding: 2 * Appearance.factor
            leftPadding: 0
            text: qsTr("Small letters (a-z)")
            checked: true
            enabled: !checked || generatorDialog.kindsTicked > 1
            onToggled: generatorDialog.makeOne()
        }
        CheckBox {
            id: digits
            // 22 high, at the left edge (D-082).
            topPadding: 2 * Appearance.factor
            bottomPadding: 2 * Appearance.factor
            leftPadding: 0
            text: qsTr("Digits (0-9)")
            checked: true
            enabled: !checked || generatorDialog.kindsTicked > 1
            onToggled: generatorDialog.makeOne()
        }
        CheckBox {
            id: symbols
            // 22 high, at the left edge (D-082).
            topPadding: 2 * Appearance.factor
            bottomPadding: 2 * Appearance.factor
            leftPadding: 0
            text: qsTr("Symbols (!#$%…)")
            checked: true
            enabled: !checked || generatorDialog.kindsTicked > 1
            onToggled: generatorDialog.makeOne()
        }
        }
        Button {
            text: qsTr("Another one")
            onClicked: generatorDialog.makeOne()
        }
    }
}
