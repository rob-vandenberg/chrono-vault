// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// ComboBox.qml 0.6.90
//
// --- Version history ----------------------------------------------------
// v0.6.90: Initial release (D-067)

import QtQuick
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's ComboBox, its list on the background of every pop-up (D-067).
F.ComboBox {
    id: control
    popup.background: PopupPanel {
        color: control.palette.button
        border.color: Fusion.outline(control.palette)
        shade: control.palette.shadow
    }
}
