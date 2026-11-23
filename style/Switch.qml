// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// Switch.qml 0.1.41
//
// --- Version history ----------------------------------------------------
// v0.1.41: Initial release (D-051)


import QtQuick
import QtQuick.Templates as T

// Chrono Vault's own switch, the owner's choice (D-051): a rounded track with
// a round knob. Off: a grey track, the knob at the left. On: the theme's
// blue, the knob at the right. Colour and place both say which, so it never
// reads both ways. Used wherever the program shows a switch.
T.Switch {
    id: control

    readonly property int trackWidth: Math.round(44 * UiScale.factor)
    readonly property int trackHeight: Math.round(24 * UiScale.factor)

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)
    padding: Math.round(6 * UiScale.factor)
    spacing: Math.round(8 * UiScale.factor)

    indicator: Rectangle {
        objectName: "switchTrack"
        implicitWidth: control.trackWidth
        implicitHeight: control.trackHeight
        x: control.text ? (control.mirrored ? control.leftPadding : control.width - width - control.rightPadding)
                        : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: height / 2
        // Off: the text colour laid thinly over the window, grey in both
        // themes (#44474b-like in dark, #b8b8b8-like in light).
        readonly property color offColour: Qt.tint(control.palette.window,
                                                    Qt.rgba(control.palette.windowText.r, control.palette.windowText.g,
                                                            control.palette.windowText.b, 0.2))
        color: control.checked ? control.palette.highlight : offColour
        border.width: control.checked ? 0 : 1
        border.color: Qt.darker(offColour, 1.15)
        opacity: control.enabled ? 1 : 0.5

        Rectangle {
            objectName: "switchKnob"
            readonly property int gap: Math.round(3 * UiScale.factor)
            width: parent.height - 2 * gap
            height: width
            radius: width / 2
            y: gap
            // Never mirrored: on is always the knob at the right.
            x: control.checked ? parent.width - width - gap : gap
            color: control.palette.highlightedText // white in both themes
            Behavior on x {
                NumberAnimation { duration: 120 }
            }
        }
    }

    contentItem: Text {
        leftPadding: control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.mirrored ? 0 : control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: control.palette.windowText
        opacity: control.enabled ? 1 : 0.5
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
