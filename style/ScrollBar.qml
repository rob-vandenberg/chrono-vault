// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// ScrollBar.qml 0.0.39
//
// --- Version history ----------------------------------------------------
// v0.0.39: Initial release (D-049)

import QtQuick
import QtQuick.Templates as T

// A normal scroll bar (the owner, D-049): a track, an arrow at each end that
// moves a step (and keeps moving while held), and a handle that stands out
// on the track in the light and the dark theme alike. Shown whenever the
// content is larger than its view; it never fades.
T.ScrollBar {
    id: control

    readonly property int thickness: Math.round(14 * UiScale.factor)
    readonly property int arrowLength: thickness

    implicitWidth: vertical ? thickness : Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                                  implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: vertical ? Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                        implicitContentHeight + topPadding + bottomPadding) : thickness

    // The handle moves between the two arrows.
    topPadding: vertical ? arrowLength : 2
    bottomPadding: vertical ? arrowLength : 2
    leftPadding: vertical ? 2 : arrowLength
    rightPadding: vertical ? 2 : arrowLength

    visible: policy === T.ScrollBar.AlwaysOn || (policy === T.ScrollBar.AsNeeded && size < 1.0)
    minimumSize: 0.08 // a handle long enough to see and grab
    stepSize: 0.05

    contentItem: Rectangle {
        implicitWidth: control.thickness - 4
        implicitHeight: control.thickness - 4
        radius: Math.round(2 * UiScale.factor)
        color: control.palette.text
        opacity: control.pressed ? 0.6 : (control.hovered ? 0.5 : 0.4)
    }

    background: Rectangle {
        implicitWidth: control.thickness
        implicitHeight: control.thickness
        color: control.palette.base
        border.color: control.palette.mid
    }

    // An arrow at each end; held, it keeps scrolling.
    component Arrow: Item {
        id: arrow
        property bool towardsEnd: false
        width: control.vertical ? control.width : control.arrowLength
        height: control.vertical ? control.arrowLength : control.height
        Canvas {
            id: triangle
            anchors.centerIn: parent
            width: Math.round(control.thickness * 0.64)
            height: width
            property color fill: control.palette.text
            onFillChanged: requestPaint()
            onPaint: {
                const context = getContext("2d")
                context.reset()
                context.fillStyle = fill
                context.beginPath()
                const w = width, h = height
                if (control.vertical) {
                    if (arrow.towardsEnd) {
                        context.moveTo(0, h * 0.2); context.lineTo(w, h * 0.2); context.lineTo(w / 2, h * 0.8)
                    } else {
                        context.moveTo(0, h * 0.8); context.lineTo(w, h * 0.8); context.lineTo(w / 2, h * 0.2)
                    }
                } else {
                    if (arrow.towardsEnd !== control.mirrored) {
                        context.moveTo(w * 0.2, 0); context.lineTo(w * 0.2, h); context.lineTo(w * 0.8, h / 2)
                    } else {
                        context.moveTo(w * 0.8, 0); context.lineTo(w * 0.8, h); context.lineTo(w * 0.2, h / 2)
                    }
                }
                context.closePath()
                context.fill()
            }
            opacity: arrowArea.pressed ? 0.9 : 0.6
        }
        MouseArea {
            id: arrowArea
            anchors.fill: parent
            function step() {
                if (arrow.towardsEnd)
                    control.increase()
                else
                    control.decrease()
            }
            onPressed: { step(); repeat.interval = 400; repeat.start() }
            onReleased: repeat.stop()
            onCanceled: repeat.stop()
            Timer {
                id: repeat
                repeat: true
                onTriggered: { interval = 50; arrowArea.step() }
            }
        }
    }

    Arrow {
        objectName: "scrollBarArrowBack"
        x: 0
        y: 0
    }
    Arrow {
        objectName: "scrollBarArrowForward"
        towardsEnd: true
        x: control.vertical ? 0 : control.width - width
        y: control.vertical ? control.height - height : 0
    }
}
