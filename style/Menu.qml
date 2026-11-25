// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// Menu.qml 0.6.90
//
// --- Version history ----------------------------------------------------
// v0.6.90: Initial release (D-067)

import QtQuick
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's Menu on the background of every pop-up (D-067).
F.Menu {
    id: control
    // A menu made inside a row of a list does not hear of a change of theme by
    // itself: its colours are those of what it belongs to.
    Binding {
        target: control
        property: "palette"
        value: control.parent ? control.parent.palette : undefined
        restoreMode: Binding.RestoreNone
        when: control.parent !== null
    }
    background: PopupPanel {
        color: control.palette.button
        border.color: Fusion.outline(control.palette)
        shade: control.palette.shadow
    }
}
