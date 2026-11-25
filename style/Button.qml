// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// Button.qml 0.6.92
//
// --- Version history ----------------------------------------------------
// v0.6.92: Enlarged by exactly the UI scaling (D-069)
// v0.5.82: 5 pixels free between the edge and the text, left and right (the owner)
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.impl
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's Button, its fixed sizes times the UI scaling (D-023).
F.Button {
    id: control
    Layout.preferredWidth: UiScale.whole(implicitWidth)
    Layout.preferredHeight: UiScale.whole(implicitHeight)
    padding: 4 * UiScale.factor
    // 5 pixels free at the left and right, past the edge and the light line
    // inside it, a pixel each (the owner).
    horizontalPadding: 2 + 5 * UiScale.factor
    spacing: 6 * UiScale.factor
    icon.width: Math.round(16 * UiScale.factor)
    icon.height: Math.round(16 * UiScale.factor)
    contentItem: IconText {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        icon: control.icon
        text: control.text
        font: control.font
        color: control.palette.buttonText
    }
    background: ButtonPanel {
        implicitWidth: 80 * UiScale.factor
        implicitHeight: 24 * UiScale.factor
        control: control
        visible: !control.flat || control.down || control.checked || control.highlighted || control.visualFocus
                 || (enabled && control.hovered)
    }
}
