// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// RadioButton.qml 0.0.10
//
// --- Version history ----------------------------------------------------
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's RadioButton, its fixed sizes times the UI scaling (D-023).
F.RadioButton {
    id: control
    padding: Math.round(6 * UiScale.factor)
    spacing: Math.round(6 * UiScale.factor)
    indicator: Rectangle {
        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding)
                        : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: Math.round(14 * UiScale.factor)
        implicitHeight: implicitWidth
        radius: width / 2
        color: control.down ? Qt.darker(control.palette.base, 1.1) : control.palette.base
        border.color: control.visualFocus ? control.palette.highlight : control.palette.mid
        Rectangle {
            anchors.centerIn: parent
            width: Math.round(parent.width / 2.32)
            height: width
            radius: width / 2
            color: control.palette.text
            visible: control.checked
        }
    }
}
