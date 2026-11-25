// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// SpinBox.qml 0.6.92
//
// --- Version history ----------------------------------------------------
// v0.6.92: Enlarged by exactly the UI scaling (D-069)
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.impl
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's SpinBox, its fixed sizes times the UI scaling (D-023).
F.SpinBox {
    id: control
    Layout.preferredWidth: UiScale.whole(implicitWidth)
    Layout.preferredHeight: UiScale.whole(implicitHeight)
    padding: 4 * UiScale.factor
    // The number typed, enlarged by exactly the UI scaling (D-069).
    contentItem: FocusScope {
        z: 2
        property alias text: typed.text
        readonly property alias displayText: typed.displayText
        readonly property alias acceptableInput: typed.acceptableInput
        property alias cursorPosition: typed.cursorPosition
        implicitWidth: typed.implicitWidth * UiScale.factor
        implicitHeight: typed.implicitHeight * UiScale.factor
        TextInput {
            id: typed
            focus: true
            width: parent.width / UiScale.factor
            height: parent.height / UiScale.factor
            scale: UiScale.factor
            transformOrigin: Item.TopLeft
            text: control.displayText
            font: control.font
            color: control.palette.text
            selectionColor: control.palette.highlight
            selectedTextColor: control.palette.highlightedText
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            readOnly: !control.editable
            validator: control.validator
            inputMethodHints: control.inputMethodHints
            clip: width < implicitWidth - 1
        }
    }
    up.indicator: PaddedRectangle {
        x: control.mirrored ? 1 : control.width - width - 1
        y: 1
        height: control.height / 2 - 1
        implicitWidth: Math.round(16 * UiScale.factor)
        implicitHeight: 10 * UiScale.factor
        radius: 1.7
        clip: true
        topPadding: -2
        leftPadding: -2
        color: control.up.pressed ? Fusion.buttonColor(control.palette, false, true, true) : "transparent"
        Arrow {
            rotation: 180
            width: parent.width
            height: parent.height
            opacity: enabled ? 1.0 : 0.5
            color: control.palette.buttonText
        }
    }
    down.indicator: PaddedRectangle {
        x: control.mirrored ? 1 : control.width - width - 1
        y: control.height - height - 1
        height: control.height / 2 - 1
        implicitWidth: Math.round(16 * UiScale.factor)
        implicitHeight: 10 * UiScale.factor
        radius: 1.7
        clip: true
        topPadding: -2
        leftPadding: -2
        color: control.down.pressed ? Fusion.buttonColor(control.palette, false, true, true) : "transparent"
        Arrow {
            rotation: 0
            width: parent.width
            height: parent.height
            opacity: enabled ? 1.0 : 0.5
            color: control.palette.buttonText
        }
    }
    Binding {
        target: control.background
        property: "implicitWidth"
        value: 120 * UiScale.factor
    }
    Binding {
        target: control.background
        property: "implicitHeight"
        value: 24 * UiScale.factor
    }
}
