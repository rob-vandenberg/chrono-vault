// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// MenuBarItem.qml 0.0.10
//
// --- Version history ----------------------------------------------------
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's MenuBarItem, its fixed sizes times the UI scaling (D-023).
F.MenuBarItem {
    padding: Math.round(6 * UiScale.factor)
    spacing: Math.round(6 * UiScale.factor)
    icon.width: Math.round(16 * UiScale.factor)
    icon.height: Math.round(16 * UiScale.factor)
}
