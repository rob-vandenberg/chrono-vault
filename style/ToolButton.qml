// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// ToolButton.qml 0.6.92
//
// --- Version history ----------------------------------------------------
// v0.6.92: Enlarged by exactly the UI scaling (D-069)
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.impl
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's ToolButton, its fixed sizes times the UI scaling (D-023).
F.ToolButton {
    id: control
    Layout.preferredWidth: UiScale.whole(implicitWidth)
    Layout.preferredHeight: UiScale.whole(implicitHeight)
    padding: 6 * UiScale.factor
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
        implicitWidth: 20 * UiScale.factor
        implicitHeight: 20 * UiScale.factor
        control: control
        visible: control.down || control.checked || control.highlighted || control.visualFocus
                 || (enabled && control.hovered)
    }
}
