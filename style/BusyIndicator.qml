// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// BusyIndicator.qml 0.0.10
//
// --- Version history ----------------------------------------------------
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's BusyIndicator, its fixed sizes times the UI scaling (D-023).
F.BusyIndicator {
    padding: Math.round(6 * UiScale.factor)
    implicitWidth: Math.round(28 * UiScale.factor) + leftPadding + rightPadding
    implicitHeight: Math.round(28 * UiScale.factor) + topPadding + bottomPadding
}
