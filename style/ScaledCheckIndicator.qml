// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// ScaledCheckIndicator.qml 0.0.10
//
// --- Version history ----------------------------------------------------
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// The box of a tick box or a ticked menu item, at the size of the UI scaling (D-023).
Rectangle {
    id: indicator
    property Item control
    implicitWidth: Math.round(14 * UiScale.factor)
    implicitHeight: implicitWidth
    radius: Math.max(1, Math.round(2 * UiScale.factor))
    color: control.down ? Qt.darker(control.palette.base, 1.1) : control.palette.base
    border.color: control.visualFocus ? control.palette.highlight : control.palette.mid
    ColorImage {
        anchors.centerIn: parent
        width: Math.round(parent.width * 0.8)
        height: width
        sourceSize: Qt.size(width, height)
        source: "qrc:/icons/ok.svg"
        color: indicator.control.palette.text
        visible: indicator.control.checkState === Qt.Checked
                 || (indicator.control.checked && indicator.control.checkState === undefined)
    }
}
