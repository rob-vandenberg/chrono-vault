// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg

// --- Version ------------------------------------------------------------
// IconText.qml 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Initial release (D-069)

import QtQuick
import QtQuick.Controls.impl

// An icon and a text enlarged by exactly the UI scaling, as a button shows
// them: the icon alone, the text alone, or the icon beside the text (D-069).
Item {
    id: control
    property alias icon: picture.icon
    property string text
    property alias font: words.font
    property alias color: words.color
    property real spacing: 0
    property bool mirrored: false
    property int display: IconLabel.TextBesideIcon
    property int alignment: Qt.AlignCenter

    readonly property bool hasIcon: display !== IconLabel.TextOnly && picture.implicitWidth > 0
    readonly property bool hasText: display !== IconLabel.IconOnly && text !== ""
    readonly property real gap: hasIcon && hasText ? spacing : 0
    readonly property real iconWidth: hasIcon ? Math.min(picture.implicitWidth, width) : 0
    readonly property real textWidth: hasText ? Math.max(0, Math.min(words.shownWidth, width - iconWidth - gap)) : 0
    readonly property real start: (alignment & Qt.AlignHCenter) ? (width - iconWidth - gap - textWidth) / 2
                                  : ((alignment & Qt.AlignRight) !== 0) !== mirrored ? width - iconWidth - gap - textWidth : 0
    implicitWidth: (hasIcon ? picture.implicitWidth : 0) + gap + (hasText ? words.shownWidth : 0)
    implicitHeight: Math.max(hasIcon ? picture.implicitHeight : 0, hasText ? words.shownHeight : 0)

    IconLabel {
        id: picture
        visible: control.hasIcon
        display: IconLabel.IconOnly
        color: control.color
        width: control.iconWidth
        height: Math.min(implicitHeight, control.height)
        // A picture begins at a whole pixel, so it stays sharp.
        x: Math.round(control.mirrored ? control.start + control.textWidth + control.gap : control.start)
        y: Math.round((control.height - height) / 2)
    }
    // The text alone, as Qt draws it on a button, its mnemonic underlined.
    IconLabel {
        id: words
        readonly property real shownWidth: implicitWidth * UiScale.factor
        readonly property real shownHeight: implicitHeight * UiScale.factor
        visible: control.hasText
        display: IconLabel.TextOnly
        text: control.text
        alignment: control.hasIcon ? Qt.AlignLeft : control.alignment
        mirrored: control.mirrored
        // Up to a pixel too few is no reason to cut the text.
        width: !control.hasIcon ? Math.max(control.width / UiScale.factor, control.width > shownWidth - 1.1 ? implicitWidth : 0)
               : control.textWidth > shownWidth - 1.1 ? implicitWidth : control.textWidth / UiScale.factor
        height: control.height / UiScale.factor
        scale: UiScale.factor
        transformOrigin: Item.TopLeft
        x: !control.hasIcon ? 0 : control.mirrored ? control.start : control.start + control.iconWidth + control.gap
    }
}
