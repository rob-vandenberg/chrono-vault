// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg

// --- Version ------------------------------------------------------------
// ExactColumn.qml 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Initial release (D-069)

import QtQuick
import QtQuick.Layouts

// A column that is exactly its size at 100% times the UI scaling (D-069). Qt's
// layouts round each item up to a whole pixel, which adds up over the items:
// here the sum of it comes out of the room between them. "gap" is that room.
ColumnLayout {
    id: control
    property real gap: 5 * UiScale.factor

    // An item's size along and across, as it is asked for, with its margins.
    function along(item: Item) : real {
        const at = item.Layout
        return (at.preferredHeight >= 0 ? at.preferredHeight : item.implicitHeight) + at.topMargin + at.bottomMargin
    }
    function across(item: Item) : real {
        const at = item.Layout
        return (at.preferredWidth >= 0 ? at.preferredWidth : item.implicitWidth) + at.leftMargin + at.rightMargin
    }
    // What the items are together: [the sum along, the most across, what
    // rounding adds, how many]. At 100% the layout's own.
    readonly property var measured: {
        if (UiScale.factor === 1)
            return [implicitHeight, implicitWidth, 0, 0]
        let sum = 0
        let most = 0
        let added = 0
        let count = 0
        for (const item of visibleChildren) {
            if (item instanceof Repeater)
                continue
            const size = along(item)
            sum += size
            added += Math.ceil(size) - size
            most = Math.max(most, across(item))
            ++count
        }
        return [sum + gap * Math.max(0, count - 1), most, added, count]
    }
    spacing: measured[3] > 1 ? gap - measured[2] / (measured[3] - 1) : gap
    Layout.preferredHeight: measured[0]
    Layout.preferredWidth: measured[1]
}
