// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// Menu.qml 0.7.112
//
// --- Version history ----------------------------------------------------
// v0.7.112: Its list stops at its ends, no bounce (D-046)
// v0.6.101: A menu fits its entries and closes with its window (D-072)
// v0.6.92: Enlarged by exactly the UI scaling (D-069)
// v0.6.90: Initial release (D-067)

import QtQuick
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's Menu on the background of every pop-up (D-067).
F.Menu {
    id: control
    // Its list stops at its ends, it is never pulled past them (the owner, D-046).
    Binding {
        target: control.contentItem
        property: "boundsBehavior"
        value: Flickable.StopAtBounds
        when: control.contentItem && control.contentItem.boundsBehavior !== undefined
    }
    delegate: MenuItem {}
    // As wide as its longest entry, and 120 at least (the owner, D-072).
    readonly property real longest: {
        let widest = 0
        for (let index = 0; index < count; ++index) {
            const entry = itemAt(index)
            if (entry && entry.height > 0)
                widest = Math.max(widest, entry.implicitWidth)
        }
        return widest
    }
    width: Math.max(120 * UiScale.factor, Math.ceil(longest) + leftPadding + rightPadding)
    // It is drawn inside its window and hears only of clicks there: it closes
    // when another window, or another program, takes the keyboard (D-072).
    Connections {
        target: UiScale
        enabled: control.visible
        function onFocusWindowChanged() {
            if (control.parent && UiScale.focusWindow !== control.parent.Window.window)
                control.close()
        }
    }
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
