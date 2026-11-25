// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg

// --- Version ------------------------------------------------------------
// GroupBox.qml 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Initial release (D-069)

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Fusion as F

// Fusion's GroupBox, its text and sizes times the UI scaling (D-069).
F.GroupBox {
    id: control
    Layout.preferredWidth: UiScale.whole(implicitWidth)
    Layout.preferredHeight: UiScale.whole(implicitHeight)
    spacing: 6 * UiScale.factor
    padding: 9 * UiScale.factor
    label: ScaledText {
        x: control.leftPadding
        width: control.availableWidth
        text: control.title
        font: control.font
        color: control.palette.windowText
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}
