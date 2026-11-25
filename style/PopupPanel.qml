// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// PopupPanel.qml 0.6.90
//
// --- Version history ----------------------------------------------------
// v0.6.90: Initial release (D-067)

import QtQuick

// What every pop-up menu and list stands on (D-067): the buttons' colour, a
// step lighter than the sidebar and the window in the dark theme and a step
// darker in the light one, so it is told apart from what it lies on; with
// Fusion's outline and shadow. Each colour is bound where the panel is used:
// a palette handed over whole does not follow a change of theme.
Rectangle {
    id: panel
    property color shade // Fusion's shadow
    implicitWidth: 200
    implicitHeight: 20
    Rectangle {
        z: -1
        x: 1; y: 1
        width: parent.width
        height: parent.height
        color: panel.shade
        opacity: 0.2
    }
}
