// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// DialogButtonBox.qml 0.6.92
//
// --- Version history ----------------------------------------------------
// v0.6.92: Enlarged by exactly the UI scaling (D-069)
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.impl
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's DialogButtonBox, its fixed sizes times the UI scaling (D-023).
F.DialogButtonBox {
    Layout.preferredWidth: UiScale.whole(implicitWidth)
    Layout.preferredHeight: UiScale.whole(implicitHeight)
    // Its buttons as Fusion's, 4 pixels free at their sides.
    delegate: Button {
        horizontalPadding: 4 * UiScale.factor
    }
    padding: 6 * UiScale.factor
    spacing: 6 * UiScale.factor
}
