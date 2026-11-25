// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg

// --- Version ------------------------------------------------------------
// Arrow.qml 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Initial release (D-069)

import QtQuick
import QtQuick.Controls.impl

// Fusion's arrow of choice boxes, number boxes and menus, times the UI scaling:
// its own picture at 100%, its largest otherwise (D-069).
Item {
    property alias color: picture.color
    implicitWidth: 20 * UiScale.factor
    implicitHeight: picture.height
    readonly property string pictures: "qrc:/qt-project.org/imports/QtQuick/Controls/Fusion/images/"
    readonly property bool own: UiScale.factor === 1
    ColorImage {
        id: picture
        // Begins at a whole pixel, so it stays sharp.
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        source: parent.pictures + (parent.own ? "arrow.png" : "arrow@4x.png")
        width: parent.own ? implicitWidth : Math.round(sourceSize.width / 4 * UiScale.factor)
        height: parent.own ? implicitHeight : Math.round(sourceSize.height / 4 * UiScale.factor)
        mipmap: !parent.own
    }
}
