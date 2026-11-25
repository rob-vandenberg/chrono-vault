// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// BusyIndicator.qml 0.6.92
//
// --- Version history ----------------------------------------------------
// v0.6.92: Enlarged by exactly the UI scaling (D-069)
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's BusyIndicator, its fixed sizes times the UI scaling (D-023).
F.BusyIndicator {
    padding: 6 * UiScale.factor
    implicitWidth: 28 * UiScale.factor + leftPadding + rightPadding
    implicitHeight: 28 * UiScale.factor + topPadding + bottomPadding
}
