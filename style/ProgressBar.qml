// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg

// --- Version ------------------------------------------------------------
// ProgressBar.qml 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Initial release (D-069)

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Fusion as F

// Fusion's ProgressBar, its size times the UI scaling (D-069).
F.ProgressBar {
    Layout.preferredWidth: UiScale.whole(implicitWidth)
    Layout.preferredHeight: UiScale.whole(implicitHeight)
    implicitWidth: 120 * UiScale.factor
    implicitHeight: 24 * UiScale.factor
}
