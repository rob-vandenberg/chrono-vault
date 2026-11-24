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
// GeneratorDialog.qml 0.5.78

// --- Version history ----------------------------------------------------
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

    title: qsTr("Generate a password")
    standardButtons: Dialog.Ok | Dialog.Cancel

    onShown: makeOne()
    onAccepted: usePassword(password)

    ColumnLayout {
        spacing: Math.round(8 * Appearance.factor)

        TextField {
            text: generatorDialog.password
            readOnly: true
            font.family: "Noto Sans Mono"
            selectByMouse: true
            Layout.preferredWidth: Math.round(420 * Appearance.factor)
            Layout.fillWidth: true
        }
        RowLayout {
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
        CheckBox {
            id: uppercase
            text: qsTr("Capital letters (A-Z)")
            checked: true
            enabled: !checked || generatorDialog.kindsTicked > 1
            onToggled: generatorDialog.makeOne()
        }
        CheckBox {
            id: lowercase
            text: qsTr("Small letters (a-z)")
            checked: true
            enabled: !checked || generatorDialog.kindsTicked > 1
            onToggled: generatorDialog.makeOne()
        }
        CheckBox {
            id: digits
            text: qsTr("Digits (0-9)")
            checked: true
            enabled: !checked || generatorDialog.kindsTicked > 1
            onToggled: generatorDialog.makeOne()
        }
        CheckBox {
            id: symbols
            text: qsTr("Symbols (!#$%…)")
            checked: true
            enabled: !checked || generatorDialog.kindsTicked > 1
            onToggled: generatorDialog.makeOne()
        }
        Button {
            text: qsTr("Another one")
            onClicked: generatorDialog.makeOne()
        }
    }
}
