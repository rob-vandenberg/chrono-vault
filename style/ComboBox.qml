// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
//
// --- Version ------------------------------------------------------------
// ComboBox.qml 0.6.92
//
// --- Version history ----------------------------------------------------
// v0.6.92: Enlarged by exactly the UI scaling (D-069)
// v0.6.90: Initial release (D-067)

import QtQuick
import QtQuick.Layouts
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
    Layout.preferredWidth: UiScale.whole(implicitWidth)
    Layout.preferredHeight: UiScale.whole(implicitHeight)
    delegate: MenuItem {
        required property var model
        required property int index
        width: ListView.view.width
        text: model[control.textRole]
        font.weight: control.currentIndex === index ? Font.DemiBold : Font.Normal
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
    }
    indicator: Arrow {
        x: control.mirrored ? control.padding : control.width - width - control.padding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: implicitWidth
        color: control.palette.buttonText
    }
    // No choice box of Chrono Vault's is typed in.
    contentItem: ScaledText {
        leftPadding: 4 * UiScale.factor - control.padding
        rightPadding: 4 * UiScale.factor - control.padding
        text: control.displayText
        font: control.font
        textFormat: Text.PlainText
        color: control.palette.buttonText
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        Rectangle {
            x: 1 - control.leftPadding
            y: 1 - control.topPadding
            width: control.width - 2
            height: control.height - 2
            color: "transparent"
            border.color: Qt.alpha(Fusion.highlightedOutline(control.palette), 40 / 255)
            visible: control.activeFocus
            radius: 1.7
        }
    }
    background: ButtonPanel {
        implicitWidth: 120 * UiScale.factor
        implicitHeight: 24 * UiScale.factor
        control: control
        visible: !control.flat || control.down
        highlighted: control.visualFocus || control.contentItem.activeFocus
    }
}
