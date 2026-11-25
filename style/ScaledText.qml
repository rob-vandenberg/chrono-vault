// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg

// --- Version ------------------------------------------------------------
// ScaledText.qml 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Initial release (D-069)

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

// A text drawn at its size at 100% and enlarged by exactly the UI scaling: a
// font itself only comes in whole pixels, so 110% would draw 115% (D-069).
// Its paddings and its sizes are those seen on the screen.
Item {
    id: control
    property alias text: words.text
    property alias font: words.font
    property alias color: words.color
    property alias linkColor: words.linkColor
    property alias textFormat: words.textFormat
    property alias wrapMode: words.wrapMode
    property alias elide: words.elide
    property alias maximumLineCount: words.maximumLineCount
    property alias horizontalAlignment: words.horizontalAlignment
    property int verticalAlignment: Text.AlignTop
    property real padding: 0
    property real leftPadding: padding
    property real rightPadding: padding
    property real topPadding: padding
    property real bottomPadding: padding
    readonly property alias palette: words.palette
    readonly property alias truncated: words.truncated
    readonly property alias lineCount: words.lineCount
    readonly property alias hoveredLink: words.hoveredLink
    readonly property real contentWidth: words.contentWidth * UiScale.factor
    readonly property real contentHeight: words.contentHeight * UiScale.factor
    signal linkActivated(string link)

    // Noted when the text says them, not asked for: asked for inside a binding,
    // a text works them out there and then, and the binding goes in a circle.
    property real wordsWidth: 0
    property real wordsHeight: 0
    implicitWidth: wordsWidth * UiScale.factor + leftPadding + rightPadding
    implicitHeight: wordsHeight * UiScale.factor + topPadding + bottomPadding
    baselineOffset: topPadding + words.baselineOffset * UiScale.factor
    // A layout rounds what it takes by itself up to a whole pixel, and over
    // many rows that adds up; what it is told it keeps.
    Layout.preferredWidth: UiScale.whole(implicitWidth)
    Layout.preferredHeight: UiScale.whole(implicitHeight)

    // The room in the text's own pixels, followed when the room changes, and
    // only after everything else when the scaling does: in between, the room
    // is still the one of the scaling before, and the text would break anew for
    // nothing. A layout gives whole pixels, so up to one too few is no reason
    // to cut or break the text.
    function fit() {
        const own = (width - leftPadding - rightPadding) / UiScale.factor
        words.width = own < wordsWidth && own > wordsWidth - 1.1 / UiScale.factor ? wordsWidth : own
    }
    // A text that breaks grows in height with its room: after the layout that
    // gave the room, never in the middle of it.
    onWidthChanged: wrapMode === Text.NoWrap ? fit() : Qt.callLater(fit)
    onWordsWidthChanged: fit()
    onLeftPaddingChanged: Qt.callLater(fit)
    onRightPaddingChanged: Qt.callLater(fit)

    T.Label {
        id: words
        // Its place, not its height, follows the room: a height given would go
        // round in circles with a text that breaks.
        x: control.leftPadding
        y: control.topPadding + (control.verticalAlignment === Text.AlignVCenter ? (control.height - control.implicitHeight) / 2
                                 : control.verticalAlignment === Text.AlignBottom ? control.height - control.implicitHeight : 0)
        scale: UiScale.factor
        onScaleChanged: Qt.callLater(control.fit)
        transformOrigin: Item.TopLeft
        onImplicitWidthChanged: control.wordsWidth = implicitWidth
        onImplicitHeightChanged: control.wordsHeight = implicitHeight
        Component.onCompleted: {
            control.wordsWidth = implicitWidth
            control.wordsHeight = implicitHeight
            control.fit()
        }
        color: palette.windowText
        linkColor: palette.link
        onLinkActivated: function(link) { control.linkActivated(link) }
    }
}
