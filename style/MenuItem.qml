// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// MenuItem.qml 0.0.10
//
// --- Version history ----------------------------------------------------
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's MenuItem, its fixed sizes times the UI scaling (D-023).
F.MenuItem {
    id: control
    padding: Math.round(6 * UiScale.factor)
    spacing: Math.round(6 * UiScale.factor)
    icon.width: Math.round(16 * UiScale.factor)
    icon.height: Math.round(16 * UiScale.factor)
    indicator: ScaledCheckIndicator {
        x: control.mirrored ? control.width - width - control.rightPadding : control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        visible: control.checkable
        control: control
    }
}
