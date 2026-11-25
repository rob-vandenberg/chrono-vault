// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg

// --- Version ------------------------------------------------------------
// MenuSeparator.qml 0.6.101

// --- Version history ----------------------------------------------------
// v0.6.101: A menu fits its entries and closes with its window (D-072)
// v0.6.92: Initial release (D-069)

import QtQuick
import QtQuick.Controls.Fusion as F

// Fusion's MenuSeparator, its sizes times the UI scaling (D-069).
F.MenuSeparator {
    padding: 5 * UiScale.factor
    id: control
    verticalPadding: 1
    Binding {
        target: control.contentItem
        property: "implicitWidth"
        value: 0
    }
}
