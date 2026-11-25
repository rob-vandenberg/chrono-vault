// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg

// --- Version ------------------------------------------------------------
// MenuSeparator.qml 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Initial release (D-069)

import QtQuick
import QtQuick.Controls.Fusion as F

// Fusion's MenuSeparator, its sizes times the UI scaling (D-069).
F.MenuSeparator {
    padding: 5 * UiScale.factor
    verticalPadding: 1
}
