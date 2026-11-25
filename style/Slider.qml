// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// Slider.qml 0.6.92
//
// --- Version history ----------------------------------------------------
// v0.6.92: Enlarged by exactly the UI scaling (D-069)
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.impl
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's Slider, its fixed sizes times the UI scaling (D-023).
F.Slider {
    id: control
    // Its handle is drawn in whole pixels; the room for it is exact.
    implicitHeight: (horizontal ? 13 * UiScale.factor : implicitBackgroundHeight) + topPadding + bottomPadding
    Layout.preferredWidth: UiScale.whole(implicitWidth)
    Layout.preferredHeight: UiScale.whole(implicitHeight)
    handle: Rectangle {
        x: control.leftPadding + Math.round(control.horizontal ? control.visualPosition * (control.availableWidth - width)
                                                                : (control.availableWidth - width) / 2)
        y: control.topPadding + Math.round(control.horizontal ? (control.availableHeight - height) / 2
                                                               : control.visualPosition * (control.availableHeight - height))
        implicitWidth: Math.round(13 * UiScale.factor)
        implicitHeight: implicitWidth
        radius: Math.round(2 * UiScale.factor)
        color: control.pressed ? Qt.darker(control.palette.button, 1.1) : control.palette.button
        border.color: control.visualFocus ? control.palette.highlight : control.palette.mid
    }
    background: Rectangle {
        x: control.leftPadding + (control.horizontal ? 0 : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 : 0)
        implicitWidth: control.horizontal ? Math.round(160 * UiScale.factor) : Math.round(5 * UiScale.factor)
        implicitHeight: control.horizontal ? 5 * UiScale.factor : 160 * UiScale.factor
        width: control.horizontal ? control.availableWidth : implicitWidth
        height: control.horizontal ? implicitHeight : control.availableHeight
        radius: Math.round(2 * UiScale.factor)
        color: Qt.darker(control.palette.window, 1.1)
        border.color: control.palette.mid
        Rectangle {
            width: control.horizontal ? control.position * parent.width : parent.width
            height: control.horizontal ? parent.height : control.position * parent.height
            y: control.horizontal ? 0 : parent.height - height
            radius: parent.radius
            color: control.palette.highlight
        }
    }
}
