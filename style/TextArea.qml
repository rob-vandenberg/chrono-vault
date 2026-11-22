// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// TextArea.qml 0.0.10
//
// --- Version history ----------------------------------------------------
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's TextArea, its fixed sizes times the UI scaling (D-023).
F.TextArea {
    padding: Math.round(6 * UiScale.factor)
    leftPadding: padding + Math.round(4 * UiScale.factor)
}
