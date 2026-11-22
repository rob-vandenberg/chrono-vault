// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// ScrollView.qml 0.0.36
//
// --- Version history ----------------------------------------------------
// v0.0.36: Initial release (D-046)

import QtQuick
import QtQuick.Controls.Fusion as F

// Fusion's ScrollView without the bounce: the page stops at its ends, it is
// never pulled past them to spring back (the owner, D-046).
F.ScrollView {
    id: control
    Binding {
        target: control.contentItem
        property: "boundsBehavior"
        value: Flickable.StopAtBounds
        when: control.contentItem && control.contentItem.boundsBehavior !== undefined
    }
}
