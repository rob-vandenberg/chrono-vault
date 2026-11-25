// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// MenuItem.qml 0.6.101
//
// --- Version history ----------------------------------------------------
// v0.6.101: A menu fits its entries and closes with its window (D-072)
// v0.6.92: Enlarged by exactly the UI scaling (D-069)
// v0.0.24: The action's shortcut shown at the right (D-039)
// v0.0.10: Initial release (D-023)

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Controls.Fusion as F
import QtQuick.Controls.Fusion.impl

// Fusion's MenuItem, its fixed sizes times the UI scaling (D-023).
F.MenuItem {
    id: control
    padding: 6 * UiScale.factor
    spacing: 6 * UiScale.factor
    icon.width: Math.round(16 * UiScale.factor)
    icon.height: Math.round(16 * UiScale.factor)
    // The text, and the action's shortcut at the right, as menus show it
    // (D-039).
    // As the menu writes it ("Shift+Ctrl+U", as the reference program shows
    // it); a key sequence given otherwise in the system's own words.
    // An action may give the text itself in shownKeys, so it stays shown
    // while its shortcut stands aside (a text field being typed in).
    readonly property string shortcutText: !control.action ? ""
                                           : typeof control.action.shownKeys === "string" ? control.action.shownKeys
                                           : !control.action.shortcut ? ""
                                           : typeof control.action.shortcut === "string" ? control.action.shortcut
                                           : keySequence.nativeText
    // Only to give the text of a shortcut not given as words.
    Shortcut {
        id: keySequence
        enabled: false
        sequence: control.action && control.action.shortcut ? control.action.shortcut : ""
    }
    contentItem: Item {
        readonly property real indicatorPadding: control.checkable && control.indicator
                                                 ? control.indicator.width + control.spacing : 0
        readonly property real arrowPadding: control.subMenu && control.arrow ? control.arrow.width + control.spacing : 0
        implicitWidth: indicatorPadding + label.implicitWidth
                       + (keys.text !== "" ? 32 * UiScale.factor + keys.implicitWidth : 0) + arrowPadding
        implicitHeight: Math.max(label.implicitHeight, keys.implicitHeight)
        IconText {
            id: label
            x: control.mirrored ? parent.width - width - parent.indicatorPadding : parent.indicatorPadding
            y: (parent.height - height) / 2
            spacing: control.spacing
            mirrored: control.mirrored
            display: control.display
            alignment: Qt.AlignLeft
            icon: control.icon
            text: control.text
            font: control.font
            color: control.down || control.highlighted ? Fusion.highlightedText(control.palette) : control.palette.text
        }
        ScaledText {
            id: keys
            objectName: "menuShortcut"
            x: control.mirrored ? parent.arrowPadding : parent.width - width - parent.arrowPadding
            y: (parent.height - height) / 2
            text: control.shortcutText
            font: control.font
            color: control.down || control.highlighted ? Fusion.highlightedText(control.palette) : control.palette.text
            opacity: 0.7
        }
    }
    indicator: ScaledCheckIndicator {
        x: control.mirrored ? control.width - width - control.rightPadding : control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        visible: control.checkable
        control: control
    }
    arrow: Arrow {
        x: control.mirrored ? control.padding : control.width - width - control.padding
        y: control.topPadding + (control.availableHeight - height) / 2
        visible: control.subMenu
        rotation: control.mirrored ? 90 : -90
        color: control.down || control.hovered || control.highlighted ? Fusion.highlightedText(control.palette)
                                                                      : control.palette.text
    }
    background: Rectangle {
        implicitWidth: 0 // the menu decides how wide it is
        implicitHeight: 20 * UiScale.factor
        color: Fusion.highlight(control.palette)
        visible: control.down || control.highlighted
    }
}
