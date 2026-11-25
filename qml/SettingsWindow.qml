// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg <RobVandenberg64@gmail.com>
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License version 3 as
// published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
// General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// --- Version ------------------------------------------------------------
// SettingsWindow.qml 0.6.97

// --- Version history ----------------------------------------------------
// v0.6.97: A long text breaks in a window of normal width (D-061)
// v0.6.95: UI scaling to 200%, Options in Tools, Lock vault in File, first sizes and headings (the owner)
// v0.6.94: The main window keeps its size, the others scale along (D-070)
// v0.6.92: Exact sizes (D-069)
// v0.6.91: More room for the last two lines of General; 720 by 640 inside (D-068)
// v0.6.89: 720 by 625 inside, opened and at least (the owner, D-066)
// v0.6.88: The topics Categories, Tags and Templates; Customize and Clipboard gone (D-066)
// v0.6.86: The check for leaked passwords (Security) (D-065)
// v0.5.83: showCustomize (D-063)
// v0.5.82: Vaults: Create vault and Open vault, the crown, no Restore defaults; its keys only while it has the keyboard (D-062)
// v0.5.78: Texts in dialogs wrap with the window (D-061)
// v0.5.77: Vaults: Import… and Export… for any vault, unlocked first with its master password (D-060)
// v0.5.75: Vaults: a menu per vault with Lock, Move…, Forget… and Disconnect…; vaults not found listed (D-060)
// v0.4.70: Security in the owner's order; the PIN line (D-059)
// v0.3.60: The Vaults topic: where each vault is kept, its last sync, Move to WebDAV… and Disconnect… (D-057)
// v0.2.53: Windows Hello: no popup on Windows, a red line at the bottom left while it is open (the owner)
// v0.2.52: The reminder text "again, once every"; Change right after its text; the clipboard field 40 narrower (the owner)
// v0.2.49: The reminder box as wide as its longest word; the minutes fields 40 narrower (the owner)
// v0.1.44: The key box and the question about a key in use as windows of their own, as Chrono Viewer's (D-053)
// v0.0.40: The reminder just below Change the Master Password (D-050)
// v0.0.39: Autolock tick boxes in a frame; the reminder a choice; the clipboard on one line; a normal scroll bar (D-049)
// v0.0.38: "Use the Esc key to exit the program" in General (D-048)
// v0.0.37: The shortcuts page as Chrono Viewer's, square × and +; a topic as tall as itself (D-047)
// v0.0.35: Keyboard shortcuts, after Chrono Viewer's page (D-045)
// v0.0.28: The fingerprint one check box for all vaults, its touch in a popup; the switches no longer mirrored (D-043)
// v0.0.27: General: Keep running in the tray last; the fingerprint a switch, its touch and outcome told here (D-042)
// v0.0.25: Security: Change the Master Password, Autolock when (D-040)
// v0.0.23: Unlocking with a fingerprint, and the yearly master password reminder (Security) (D-038)
// v0.0.20: Use website icons (Customize), off by default (D-036)
// v0.0.17: Hide sensitive fields (Security) and Show subtitles (Customize) (D-033)
// v0.0.14: The Customize topic: the categories and templates of the open vault (D-027)
// v0.0.13: Its size includes the title bar and frame (the owner)
// v0.0.12: Its size gives way to a screen too small for it (the owner)
// v0.0.11: Opens at 730 by 650 and is never smaller; the scaling slider's marks every 25% above it, room around it (the owner)
// v0.0.10: The program's colours from the start; the slider in steps of 5%; every size times the UI scaling (D-023)
// v0.0.9: Opened at General from the View menu (D-025)
// v0.0.8: "Keep Chrono Vault running in the system tray"
// v0.0.7: Theme and UI scaling in General, shown at once, kept with OK; the pages scroll (D-023)
// v0.0.6: The sidebar as a framed panel; OK, Cancel and Restore defaults (D-022)
// v0.0.5: "Check for updates" in General (D-021)
// v0.0.4: The version in the title; the General topic
// v0.0.3: Initial release

// Every setting in one window: the topics on the left, the chosen topic's
// settings on the right (D-011). OK keeps the changes of every topic, Cancel
// (or closing the window) throws them away, Restore defaults puts back the
// topic being shown, to be kept with OK (D-022).
ApplicationWindow {
    id: settingsWindow
    // The program's font, so the UI scaling reaches every control (D-023).
    font: Appearance.font
    // The program's colours, never the desktop theme's (D-023).
    // The size set is the window's inside, so General just fits under any
    // title bar (the owner, D-066); the frame the desktop draws, known once
    // it is shown, is only kept on the screen.
    property size frame: Qt.size(0, 0)
    onFrameSwapped: {
        if (frame.width === 0 && frame.height === 0) {
            const drawn = Appearance.frameOf(this)
            if (drawn.width > 0 || drawn.height > 0)
                frame = drawn
        }
    }
    Component.onCompleted: Appearance.dress(this)
    objectName: "settingsWindow"

    function showWindow() {
        if (!visible) {
            loadSettings()
            // Each opening starts on the topics, never on a button left
            // with the focus (Enter would press it).
            topics.forceActiveFocus()
        }
        show()
        raise()
        requestActivate()
    }

    // From the audit (D-065).
    function showSecurity() {
        showWindow()
        topics.currentIndex = 1
    }

    // From the vault's counts (D-063).
    function showCategories() {
        showWindow()
        topics.currentIndex = 3
    }
    function showTags() {
        showWindow()
        topics.currentIndex = 4
    }

    // Help > Keyboard shortcuts… (D-045).
    function showShortcuts() {
        showWindow()
        topics.currentIndex = 6
    }

    // The small square buttons of the Keyboard shortcuts page: × and +, a grey
    // fill, as on Chrono Viewer's page (D-047).
    component KeyButton: Button {
        implicitWidth: 24 * Appearance.factor
        implicitHeight: 24 * Appearance.factor
        padding: 0
        font.pixelSize: 14
        background: Rectangle {
            radius: Math.round(3 * Appearance.factor)
            color: parent.down ? settingsWindow.palette.mid : settingsWindow.palette.button
            border.color: settingsWindow.palette.mid
        }
    }

    // The actions whose keys the person sets, from the main window, so the
    // page uses the menus' own words (D-045).
    property var keyedActions: []
    // The keys shown, by action, until OK keeps them.
    property var keyMap: ({})
    // While the box waits for a key, or asks about a key in use, no shortcut
    // answers anywhere: every key is the new key (P29).
    readonly property bool catchingAKey: keyCatcher.visible || keyTaken.visible
    function loadKeys() {
        const map = {}
        for (const action of keyedActions)
            map[action.actionId] = VaultController.keysOf(action.actionId)
        keyMap = map
    }
    function keepKeys() {
        for (const action of keyedActions) {
            const keys = keyMap[action.actionId] || []
            if (JSON.stringify(keys) !== JSON.stringify(VaultController.keysOf(action.actionId)))
                VaultController.setKeys(action.actionId, keys)
        }
    }
    // Who answers to a key now, other than this action; "" for nobody.
    function holderOf(key, exceptAction) {
        for (const action of keyedActions)
            if (action.actionId !== exceptAction && (keyMap[action.actionId] || []).indexOf(key) >= 0)
                return action.actionId
        return ""
    }
    // An action's words as its menu shows them.
    function nameOf(actionId) {
        for (const action of keyedActions)
            if (action.actionId === actionId)
                return action.text.replace("&", "")
        return actionId
    }
    function giveKey(actionId, key) {
        const map = Object.assign({}, keyMap)
        const holder = holderOf(key, actionId)
        if (holder !== "")
            map[holder] = map[holder].filter(function(one) { return one !== key }) // only that one key
        if ((map[actionId] || []).indexOf(key) < 0)
            map[actionId] = (map[actionId] || []).concat([key])
        keyMap = map
    }
    function takeKey(actionId, key) {
        const map = Object.assign({}, keyMap)
        map[actionId] = (map[actionId] || []).filter(function(one) { return one !== key })
        keyMap = map
    }

    // Opened at General (the View menu).
    function showGeneral() {
        showWindow()
        topics.currentIndex = 0
    }

    // What the window shows, until OK keeps it.
    property bool keepInTray: false
    property bool startAtStartup: false
    property bool startMinimized: false
    property bool escapeExits: false
    property int checkFrequency: 0
    property int autoLockMinutes: 0
    // Autolock when (D-040).
    property bool idleLockOn: true
    property bool lockWhenClosed: false
    property bool lockOnSleep: false
    property bool systemIdleLockOn: false
    property int systemIdleMinutes: 10
    // Security > Change the Master Password, opened by the main window.
    signal changeMasterPasswordRequested()
    // Import… and Export… of a vault's menu, once that vault is unlocked (D-060).
    signal createVaultRequested()
    signal openVaultRequested()
    readonly property bool onVaults: topics.currentIndex === 2
    // What Restore defaults would change is on this topic (D-022, D-066).
    readonly property bool restorable: topics.currentIndex < 2 || topics.currentIndex === 6
    signal importRequested()
    signal exportRequested()
    property int clipboardClearSeconds: 0
    property bool hideSensitive: true
    property bool showSubtitles: true
    property bool useWebsiteIcons: false
    property bool checkLeaks: false
    property int masterPasswordReminder: 2
    // A PIN wanted; OK keeps it only with a PIN set or one long enough typed.
    property bool pinWanted: false
    readonly property bool pinAcceptable: !pinWanted
        || (pinField.text === "" ? VaultController.pinSet : pinField.text.length >= VaultController.pinShortest)
    // Shown at once while the window is open; kept with OK, put back otherwise.
    property int theme: 2
    property int scale: 100

    function loadSettings() {
        theme = Appearance.theme
        scale = Appearance.scale
        keepInTray = VaultController.keepInTray
        startAtStartup = VaultController.startAtStartup
        startMinimized = VaultController.startMinimized
        escapeExits = VaultController.escapeExits
        checkFrequency = Updater.frequency
        autoLockMinutes = VaultController.autoLockMinutes
        idleLockOn = VaultController.idleLockOn
        lockWhenClosed = VaultController.lockWhenClosed
        lockOnSleep = VaultController.lockOnSleep
        systemIdleLockOn = VaultController.systemIdleLockOn
        systemIdleMinutes = VaultController.systemIdleMinutes
        clipboardClearSeconds = VaultController.clipboardClearSeconds
        hideSensitive = VaultController.hideSensitive
        showSubtitles = VaultController.showSubtitles
        useWebsiteIcons = VaultController.useWebsiteIcons
        checkLeaks = VaultController.checkLeaks
        masterPasswordReminder = VaultController.masterPasswordReminder
        pinWanted = VaultController.pinSet
        pinField.clear()
        pinField.revealed = false
        loadKeys()
    }

    function showTheme(chosen) {
        theme = chosen
        Appearance.show(theme, scale)
    }
    function showScale(percent) {
        scale = percent
        Appearance.show(theme, scale)
    }
    // Room for the longest percentage, in the font in use.
    Label {
        id: widestPercent
        visible: false
        text: qsTr("%1%").arg(Appearance.largestScale)
        font: Appearance.font
    }

    // OK: every topic's changes are kept, whichever topic is shown.
    function accept() {
        if (!pinAcceptable)
            return
        // A number still being typed counts.
        okButton.forceActiveFocus()
        if (pinWanted && pinField.text !== "")
            VaultController.setPin(pinField.text)
        else if (!pinWanted && VaultController.pinSet)
            VaultController.setPin("")
        pinField.clear()
        if (Appearance.theme !== theme || Appearance.scale !== scale)
            Appearance.keep(theme, scale)
        if (VaultController.keepInTray !== keepInTray)
            VaultController.keepInTray = keepInTray
        if (VaultController.startAtStartup !== startAtStartup)
            VaultController.startAtStartup = startAtStartup
        if (VaultController.startMinimized !== startMinimized)
            VaultController.startMinimized = startMinimized
        if (VaultController.escapeExits !== escapeExits)
            VaultController.escapeExits = escapeExits
        if (Updater.frequency !== checkFrequency)
            Updater.frequency = checkFrequency
        if (VaultController.autoLockMinutes !== autoLockMinutes)
            VaultController.autoLockMinutes = autoLockMinutes
        if (VaultController.idleLockOn !== idleLockOn)
            VaultController.idleLockOn = idleLockOn
        if (VaultController.lockWhenClosed !== lockWhenClosed)
            VaultController.lockWhenClosed = lockWhenClosed
        if (VaultController.lockOnSleep !== lockOnSleep)
            VaultController.lockOnSleep = lockOnSleep
        if (VaultController.systemIdleLockOn !== systemIdleLockOn)
            VaultController.systemIdleLockOn = systemIdleLockOn
        if (VaultController.systemIdleMinutes !== systemIdleMinutes)
            VaultController.systemIdleMinutes = systemIdleMinutes
        if (VaultController.clipboardClearSeconds !== clipboardClearSeconds)
            VaultController.clipboardClearSeconds = clipboardClearSeconds
        if (VaultController.hideSensitive !== hideSensitive)
            VaultController.hideSensitive = hideSensitive
        if (VaultController.showSubtitles !== showSubtitles)
            VaultController.showSubtitles = showSubtitles
        if (VaultController.useWebsiteIcons !== useWebsiteIcons)
            VaultController.useWebsiteIcons = useWebsiteIcons
        if (VaultController.checkLeaks !== checkLeaks)
            VaultController.checkLeaks = checkLeaks
        if (VaultController.masterPasswordReminder !== masterPasswordReminder)
            VaultController.masterPasswordReminder = masterPasswordReminder
        keepKeys()
        // Starting at startup is written outside Chrono Vault and can fail:
        // the window stays open and says so, showing the setting as it is.
        if (VaultController.startAtStartup !== startAtStartup) {
            startAtStartup = VaultController.startAtStartup
            topics.currentIndex = 0
            problem = qsTr("Chrono Vault could not be set to start at startup. The other settings are kept.")
            return
        }
        close()
    }

    // On Windows, Windows Hello shows its own window with its own Cancel: no
    // popup here, only a line at the bottom left while it is open and what
    // came of it (the owner, D-038).
    property bool helloOnTheLine: Qt.platform.os === "windows"

    // Ticking the fingerprint: "Touch the fingerprint reader…" with Cancel,
    // then what came of it, in a popup (the owner, D-043).
    Popup {
        objectName: "quickUnlockPopup"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.NoAutoClose
        visible: settingsWindow.visible && !settingsWindow.helloOnTheLine
                 && (VaultController.quickUnlockBusy || VaultController.quickUnlockMessage !== "")
        padding: 16 * Appearance.factor
        contentItem: ExactColumn {
            gap: 12 * Appearance.factor
            Label {
                objectName: "quickUnlockMessage"
                text: VaultController.quickUnlockMessage
                font.bold: VaultController.quickUnlockTrouble
                wrapMode: Text.Wrap
                Layout.maximumWidth: 360 * Appearance.factor
            }
            Button {
                objectName: "quickUnlockCancel"
                text: VaultController.quickUnlockBusy ? qsTr("Cancel") : qsTr("OK")
                Layout.alignment: Qt.AlignRight
                onClicked: VaultController.quickUnlockBusy ? VaultController.cancelQuickUnlockSetup()
                                                           : VaultController.clearQuickUnlockMessage()
            }
        }
    }

    // What went wrong at the last OK, shown beside the buttons.
    property string problem: ""
    // Closed without OK (Cancel, Esc, the window's own close): the theme and
    // the scale it showed are put back to what is kept.
    onVisibleChanged: {
        if (!visible) {
            problem = ""
            // A finger still asked for turning it on: the reader is given back.
            VaultController.cancelQuickUnlockSetup()
            VaultController.clearQuickUnlockMessage() // nothing left over for the next opening
            Appearance.showKept()
        }
    }

    // The topic being shown, and no other.
    function restoreDefaults() {
        const defaults = VaultController.settingsDefaults()
        switch (topics.currentIndex) {
        case 0:
            // A setting the person cannot change here is left as it is.
            if (VaultController.trayAvailable)
                keepInTray = defaults.keepInTray
            startAtStartup = defaults.startAtStartup
            startMinimized = defaults.startMinimized
            escapeExits = defaults.escapeExits
            showSubtitles = defaults.showSubtitles
            useWebsiteIcons = defaults.useWebsiteIcons
            clipboardClearSeconds = defaults.clipboardClearSeconds
            checkFrequency = Updater.defaultCheckFrequency()
            theme = Appearance.defaultTheme
            scale = Appearance.defaultScale
            Appearance.show(theme, scale)
            break
        case 1:
            autoLockMinutes = defaults.autoLockMinutes
            idleLockOn = defaults.idleLockOn
            lockWhenClosed = defaults.lockWhenClosed
            lockOnSleep = defaults.lockOnSleep
            systemIdleLockOn = defaults.systemIdleLockOn
            systemIdleMinutes = defaults.systemIdleMinutes
            hideSensitive = defaults.hideSensitive
            checkLeaks = defaults.checkLeaks
            masterPasswordReminder = defaults.masterPasswordReminder
            break
        case 6: {
            const map = {}
            for (const action of keyedActions)
                map[action.actionId] = VaultController.defaultKeysOf(action.actionId)
            keyMap = map
            break
        }
        }
    }

    title: qsTr("Chrono Vault - Settings")
    // It scales along with the UI scaling, from the size the person gave it (D-070).
    width: settingsMemory.wide(minimumWidth, minimumWidth, settingsMemory.own)
    height: settingsMemory.high(minimumHeight, minimumHeight, settingsMemory.own)
    // Never smaller than it opens (the owner).
    minimumWidth: Math.min(Math.round(720 * Appearance.factor), Appearance.screenRoom(this).width - frame.width)
    minimumHeight: Math.min(Math.round(640 * Appearance.factor), Appearance.screenRoom(this).height - frame.height)
    visible: false
    color: palette.window

    WindowMemory {
        id: settingsMemory
        window: settingsWindow
        name: "settings"
    }

    // Esc is Cancel, unless the list of "Check for updates" is open: then it
    // closes only the list.
    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: Appearance.focusWindow === settingsWindow && !frequencyBox.popup.visible && !catchingAKey
        onActivated: settingsWindow.close()
    }
    // OK is the default button: Enter keeps the changes, unless a button has
    // the keyboard focus (then Enter presses that button) or the list of
    // "Check for updates" is open (then Enter chooses in it).
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: Appearance.focusWindow === settingsWindow && !frequencyBox.popup.visible && !catchingAKey
        onActivated: {
            const focused = settingsWindow.activeFocusItem
            if (focused instanceof Button)
                focused.clicked()
            else
                settingsWindow.accept()
        }
    }

    // The topics' list is as wide as its longest name and no wider; measured
    // by the names themselves, so it follows the UI scaling.
    Column {
        id: topicNames
        objectName: "topicNames"
        visible: false
        Repeater {
            model: topics.model
            Label {
                required property string modelData
                text: modelData
                font: Appearance.font
            }
        }
    }

    ExactColumn {
        anchors.fill: parent
        anchors.margins: 10 * Appearance.factor
        gap: 10 * Appearance.factor

        ExactRow {
            Layout.fillWidth: true
            Layout.fillHeight: true
            gap: 12 * Appearance.factor

            // The topics: a framed panel, the chosen one marked across its
            // whole width.
            Rectangle {
                objectName: "topicsPanel"
                Layout.fillHeight: true
                Layout.preferredWidth: Math.ceil(topicNames.implicitWidth / Appearance.factor) * Appearance.factor + 2 * topicPadding + 2 * border.width
                readonly property real topicPadding: 6 * Appearance.factor
                color: settingsWindow.palette.base
                border.color: settingsWindow.palette.mid
                border.width: 1

                ListView {
                    id: topics
                    boundsBehavior: Flickable.StopAtBounds // no bounce (the owner, D-046)
                    objectName: "topics"
                    anchors.fill: parent
                    anchors.margins: parent.border.width
                    clip: true
                    currentIndex: 0
                    model: [qsTr("General"), qsTr("Security"), qsTr("Vaults"), qsTr("Categories"), qsTr("Tags"),
                            qsTr("Templates"), qsTr("Keyboard shortcuts")]
                    delegate: Rectangle {
                        id: topic
                        required property int index
                        required property string modelData
                        readonly property bool chosen: ListView.isCurrentItem
                        width: ListView.view.width
                        height: topicText.implicitHeight + 6 * Appearance.factor
                        color: chosen ? settingsWindow.palette.highlight : "transparent"
                        Label {
                            id: topicText
                            anchors.verticalCenter: parent.verticalCenter
                            x: 6 * Appearance.factor
                            text: topic.modelData
                            font: settingsWindow.font
                            color: topic.chosen ? settingsWindow.palette.highlightedText : settingsWindow.palette.text
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: topics.currentIndex = topic.index
                        }
                    }
                    Keys.onUpPressed: decrementCurrentIndex()
                    Keys.onDownPressed: incrementCurrentIndex()
                }
            }

            // A page taller than the window (a large UI scaling) scrolls.
            ScrollView {
                id: pageScroll
                objectName: "pageScroll"
                // A normal scroll bar with arrows (D-049), in room of its own
                // beside the page, so it never covers the + buttons; the room
                // is kept also when the page fits, so the page never depends
                // on whether the bar shows.
                rightPadding: 20 * Appearance.factor
                ScrollBar.vertical: ScrollBar {
                    id: pageScrollBar
                    objectName: "pageScrollBar"
                    parent: pageScroll
                    x: pageScroll.mirrored ? 0 : pageScroll.width - width
                    y: pageScroll.topPadding
                    height: pageScroll.height - pageScroll.topPadding
                    orientation: Qt.Vertical
                }
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                // As tall as the topic shown, not the tallest one, so a topic
                // ends at its last row (D-047).
                contentHeight: pageStack.children[pageStack.currentIndex]
                               ? pageStack.children[pageStack.currentIndex].implicitHeight + 10 * Appearance.factor
                               : 0
                clip: true

                StackLayout {
                    id: pageStack
                    currentIndex: topics.currentIndex
                    width: pageScroll.availableWidth

                    ExactColumn {
                        gap: 8 * Appearance.factor
                        Label {
                            text: qsTr("General")
                            font.pointSize: Appearance.fontPointSize * 1.4
                        }
                        Label { text: qsTr("Theme") }
                        Frame {
                            Layout.fillWidth: true
                            ExactColumn {
                                gap: 0
                                RadioButton {
                                    objectName: "lightTheme"
                                    text: qsTr("Light")
                                    checked: settingsWindow.theme === 0
                                    onToggled: settingsWindow.showTheme(0)
                                }
                                RadioButton {
                                    objectName: "darkTheme"
                                    text: qsTr("Dark")
                                    checked: settingsWindow.theme === 1
                                    onToggled: settingsWindow.showTheme(1)
                                }
                                RadioButton {
                                    objectName: "followTheme"
                                    text: qsTr("Follow the system")
                                    checked: settingsWindow.theme === 2
                                    onToggled: settingsWindow.showTheme(2)
                                }
                            }
                        }
                        ExactRow {
                            objectName: "scaleRow"
                            Layout.fillWidth: true
                            // Half a line height of room above and below (the owner).
                            Layout.topMargin: widestPercent.implicitHeight / 2
                            Layout.bottomMargin: widestPercent.implicitHeight / 2
                            gap: 12 * Appearance.factor
                            Label { text: qsTr("UI scaling") }
                            Item {
                                id: scaleTrack
                                readonly property real markHeight: 5 * Appearance.factor
                                Layout.fillWidth: true
                                // The marks on top, the slider below them.
                                implicitHeight: markHeight + 2 * Appearance.factor + scaleSlider.implicitHeight
                                Layout.preferredHeight: implicitHeight
                                Slider {
                                    id: scaleSlider
                                    objectName: "scaleSlider"
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    from: Appearance.smallestScale
                                    to: Appearance.largestScale
                                    // In steps of 5% (D-023).
                                    stepSize: Appearance.scaleStep
                                    snapMode: Slider.SnapAlways
                                    value: settingsWindow.scale
                                    // Put in use when the slider is let go, or at
                                    // once when moved with the keys.
                                    onPressedChanged: if (!pressed) settingsWindow.showScale(Math.round(value))
                                    onMoved: if (!pressed) settingsWindow.showScale(Math.round(value))
                                }
                                // A mark every 25%, above the slider.
                                Repeater {
                                    objectName: "scaleMarks"
                                    model: Math.floor((Appearance.largestScale - Appearance.smallestScale)
                                                      / Appearance.scaleMarkEvery) + 1
                                    Rectangle {
                                        required property int index
                                        readonly property real handleRoom: scaleSlider.handle ? scaleSlider.handle.width : 0
                                        x: scaleSlider.leftPadding + handleRoom / 2
                                           + index * (scaleSlider.availableWidth - handleRoom)
                                             * Appearance.scaleMarkEvery / (Appearance.largestScale - Appearance.smallestScale)
                                        y: 0
                                        width: 1
                                        height: scaleTrack.markHeight
                                        color: settingsWindow.palette.mid
                                    }
                                }
                            }
                            Label {
                                objectName: "scalePercent"
                                text: qsTr("%1%").arg(Math.round(scaleSlider.value))
                                Layout.preferredWidth: Math.ceil(widestPercent.implicitWidth)
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                        CheckBox {
                            objectName: "startAtStartupBox"
                            text: qsTr("Start Chrono Vault at startup")
                            checked: settingsWindow.startAtStartup
                            onToggled: settingsWindow.startAtStartup = checked
                        }
                        CheckBox {
                            objectName: "startMinimizedBox"
                            text: qsTr("Start Chrono Vault minimized")
                            checked: settingsWindow.startMinimized
                            onToggled: settingsWindow.startMinimized = checked
                        }
                        CheckBox {
                            objectName: "keepInTrayBox"
                            text: qsTr("Keep Chrono Vault running in the system tray")
                            checked: settingsWindow.keepInTray
                            enabled: VaultController.trayAvailable
                            onToggled: settingsWindow.keepInTray = checked
                        }
                        CheckBox {
                            objectName: "escapeExitsBox"
                            text: qsTr("Use the Esc key to exit the program")
                            checked: settingsWindow.escapeExits
                            onToggled: settingsWindow.escapeExits = checked
                        }
                        CheckBox {
                            objectName: "showSubtitlesBox"
                            text: qsTr("Show subtitles in the item list")
                            checked: settingsWindow.showSubtitles
                            onToggled: settingsWindow.showSubtitles = checked
                        }
                        CheckBox {
                            objectName: "websiteIconsBox"
                            text: qsTr("Use website icons")
                            checked: settingsWindow.useWebsiteIcons
                            onToggled: settingsWindow.useWebsiteIcons = checked
                        }
                        Label {
                            objectName: "websiteIconsRemark"
                            text: qsTr("Each website in your vault is asked for its own icon, which that website can then see. The icons are kept only while the vault is unlocked, never on the disk.")
                            wrapMode: Text.Wrap
                            opacity: 0.7
                            Layout.topMargin: -6 * Appearance.factor // close to its tick box
                            leftPadding: 28 * Appearance.factor
                            Layout.fillWidth: true
                        }
                        // One line; 0 shows as "Never" (D-049).
                        ExactRow {
                            // As far apart as the tick boxes, this line and the next (the owner).
                            Layout.topMargin: 9 * Appearance.factor
                            Label { text: qsTr("Clear the clipboard after") }
                            SpinBox {
                                objectName: "clipboardBox"
                                // 40 narrower than its own width (the owner).
                                Layout.preferredWidth: implicitWidth - 40 * Appearance.factor
                                from: 0
                                to: 600
                                value: settingsWindow.clipboardClearSeconds
                                editable: true
                                textFromValue: function(value) { return value === 0 ? qsTr("Never") : String(value) }
                                // An emptied field keeps the value it had.
                                valueFromText: function(text) {
                                    if (text === qsTr("Never"))
                                        return 0
                                    const seconds = parseInt(text, 10)
                                    return isNaN(seconds) ? value : seconds
                                }
                                validator: RegularExpressionValidator {
                                    regularExpression: new RegExp("^(\\d{1,3}|" + qsTr("Never") + ")$")
                                }
                                onValueModified: settingsWindow.clipboardClearSeconds = value
                            }
                            Label { text: qsTr("seconds") }
                        }
                        ExactRow {
                            Layout.topMargin: 5 * Appearance.factor
                            Label { text: qsTr("Check for updates") }
                            ComboBox {
                                id: frequencyBox
                                objectName: "checkFrequencyBox"
                                model: [qsTr("Daily"), qsTr("Weekly"), qsTr("Monthly"), qsTr("Never")]
                                currentIndex: settingsWindow.checkFrequency
                                onActivated: function(index) { settingsWindow.checkFrequency = index }
                            }
                        }
                        Label {
                            visible: !VaultController.trayAvailable
                            text: qsTr("This desktop has no system tray, so closing the window ends Chrono Vault.")
                            wrapMode: Text.Wrap
                            opacity: 0.7
                            Layout.fillWidth: true
                        }
                        Item { Layout.fillHeight: true }
                    }

                    ExactColumn {
                        gap: 8 * Appearance.factor
                        Label {
                            text: qsTr("Security")
                            font.pointSize: Appearance.fontPointSize * 1.4
                        }
                        // Change the Master Password, as a row of its own
                        // (the reference program's settings).
                        ExactRow {
                            Layout.fillWidth: true
                            // The button right after the text (the owner).
                            Label {
                                text: qsTr("Change the Master Password")
                            }
                            Button {
                                objectName: "changeMasterPasswordButton"
                                text: qsTr("Change")
                                enabled: VaultController.state === VaultController.Unlocked
                                onClicked: settingsWindow.changeMasterPasswordRequested()
                            }
                        }
                        // How often the master password is asked though a finger
                        // would do; Quarter by default (D-049).
                        ExactRow {
                            Layout.fillWidth: true
                            Label { text: qsTr("Ask for the master password again, once every") }
                            ComboBox {
                                id: reminderBox
                                objectName: "reminderBox"
                                model: [qsTr("Day"), qsTr("Month"), qsTr("Quarter"), qsTr("Year"), qsTr("Never")]
                                currentIndex: settingsWindow.masterPasswordReminder
                                onActivated: function(index) { settingsWindow.masterPasswordReminder = index }
                                // As wide as its longest word (the owner): closed, the
                                // word and the arrow; open, the word with 5 pixels on
                                // either side. The chosen word is shown semi-bold in
                                // the list, so that is what is measured.
                                readonly property real margin: 5 * Appearance.factor
                                readonly property real widestWord: {
                                    let widest = 0
                                    for (const word of model)
                                        widest = Math.max(widest, reminderMeasure.advanceWidth(word))
                                    return Math.ceil(widest * Appearance.factor)
                                }
                                FontMetrics {
                                    id: reminderMeasure
                                    font.family: reminderBox.font.family
                                    font.pixelSize: reminderBox.font.pixelSize > 0 ? reminderBox.font.pixelSize : -1
                                    font.pointSize: reminderBox.font.pointSize > 0 ? reminderBox.font.pointSize : -1
                                    font.weight: Font.DemiBold
                                }
                                // Fusion's text field keeps 4 pixels at either side of
                                // the word; the arrow has a slot of its own.
                                padding: 0
                                spacing: 0
                                Layout.preferredWidth: 8 * Appearance.factor + widestWord + (indicator ? indicator.width : 0)
                                popup.width: widestWord + 2 * margin + 2 * popup.padding
                                delegate: MenuItem {
                                    required property var model
                                    required property int index
                                    width: ListView.view.width
                                    leftPadding: reminderBox.margin
                                    rightPadding: reminderBox.margin
                                    text: model[reminderBox.textRole]
                                    font.weight: reminderBox.currentIndex === index ? Font.DemiBold : Font.Normal
                                    highlighted: reminderBox.highlightedIndex === index
                                    hoverEnabled: reminderBox.hoverEnabled
                                }
                            }
                        }
                        // Autolock when (D-040): four tick boxes in a frame of
                        // their own, the title on its border (D-049).
                        GroupBox {
                            objectName: "autolockGroup"
                            title: qsTr("Autolock when")
                            Layout.fillWidth: true
                            Layout.topMargin: 8 * Appearance.factor
                            ExactColumn {
                                anchors.fill: parent
                                gap: 8 * Appearance.factor
                                CheckBox {
                                    objectName: "lockWhenClosedBox"
                                    text: qsTr("Main window is closed")
                                    checked: settingsWindow.lockWhenClosed
                                    onToggled: settingsWindow.lockWhenClosed = checked
                                }
                                CheckBox {
                                    objectName: "lockOnSleepBox"
                                    text: qsTr("System sleeps")
                                    checked: settingsWindow.lockOnSleep
                                    onToggled: settingsWindow.lockOnSleep = checked
                                }
                                ExactRow {
                                    Layout.fillWidth: true
                                    CheckBox {
                                        objectName: "idleLockBox"
                                        text: qsTr("Chrono Vault is idle for")
                                        checked: settingsWindow.idleLockOn
                                        onToggled: settingsWindow.idleLockOn = checked
                                        Layout.fillWidth: true
                                    }
                                    SpinBox {
                                        objectName: "autoLockBox"
                                        from: 1
                                        to: 240
                                        value: settingsWindow.autoLockMinutes
                                        editable: true
                                        enabled: settingsWindow.idleLockOn
                                        onValueModified: settingsWindow.autoLockMinutes = value
                                        // 40 pixels narrower (the owner, 23 November 2026).
                                        Layout.preferredWidth: implicitWidth - 40 * Appearance.factor
                                    }
                                    Label { text: qsTr("Minutes") }
                                }
                                ExactRow {
                                    Layout.fillWidth: true
                                    CheckBox {
                                        objectName: "systemIdleLockBox"
                                        text: qsTr("System inactive for")
                                        checked: settingsWindow.systemIdleLockOn
                                        enabled: VaultController.systemIdleAvailable
                                        onToggled: settingsWindow.systemIdleLockOn = checked
                                        Layout.fillWidth: true
                                    }
                                    SpinBox {
                                        objectName: "systemIdleBox"
                                        from: 1
                                        to: 240
                                        value: settingsWindow.systemIdleMinutes
                                        editable: true
                                        enabled: VaultController.systemIdleAvailable && settingsWindow.systemIdleLockOn
                                        onValueModified: settingsWindow.systemIdleMinutes = value
                                        Layout.preferredWidth: implicitWidth - 40 * Appearance.factor
                                    }
                                    Label { text: qsTr("Minutes") }
                                }
                                Label {
                                    objectName: "systemIdleUnknown"
                                    visible: !VaultController.systemIdleAvailable
                                    text: qsTr("This desktop does not tell how long the system has been inactive.")
                                    wrapMode: Text.Wrap
                                    opacity: 0.7
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        // PIN unlock, after the locking, before the finger (the
                        // owner's design, D-059); kept with OK.
                        ExactRow {
                            Layout.fillWidth: true
                            Layout.topMargin: 8 * Appearance.factor
                            CheckBox {
                                id: pinBox
                                objectName: "pinBox"
                                text: qsTr("Use a PIN code to quickly unlock without having to enter the Master Password every time")
                                checked: settingsWindow.pinWanted
                                onToggled: settingsWindow.pinWanted = checked
                                Layout.fillWidth: true
                                // Room before the field, so the text breaks as the owner's design.
                                Layout.rightMargin: 40 * Appearance.factor
                                contentItem: Label {
                                    text: pinBox.text
                                    wrapMode: Text.Wrap
                                    leftPadding: pinBox.indicator.width + pinBox.spacing
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            PasswordField {
                                id: pinField
                                objectName: "pinField"
                                // Dots while a PIN is set; the PIN itself is never kept.
                                placeholderText: VaultController.pinSet ? "••••••••••••" : ""
                                // The eye only while a new PIN is typed (the owner).
                                eyeAllowed: text !== ""
                                enabled: pinBox.checked && !VaultController.pinBusy
                                Layout.preferredWidth: 140 * Appearance.factor
                                Layout.alignment: Qt.AlignVCenter
                            }
                        }
                        // One line, for all vaults (the owner, D-043): ticked, it
                        // asks for one touch in a popup, at once, not with OK.
                        CheckBox {
                            id: quickUnlockBox
                            objectName: "quickUnlockToggle"
                            text: Qt.platform.os === "windows" ? qsTr("Use Windows Hello to unlock vault")
                                                               : qsTr("Use fingerprint to unlock vault")
                            checked: VaultController.quickUnlockEnabled || VaultController.quickUnlockBusy
                            enabled: VaultController.quickUnlockAvailable && !VaultController.quickUnlockBusy
                                     && (checked || VaultController.state === VaultController.Unlocked)
                            Layout.topMargin: 8 * Appearance.factor
                            onToggled: {
                                VaultController.setQuickUnlock(checked)
                                checked = Qt.binding(function() {
                                    return VaultController.quickUnlockEnabled || VaultController.quickUnlockBusy
                                })
                            }
                        }
                        CheckBox {
                            objectName: "hideSensitiveBox"
                            text: qsTr("Hide sensitive fields: show them as dots until asked")
                            checked: settingsWindow.hideSensitive
                            onToggled: settingsWindow.hideSensitive = checked
                            Layout.topMargin: 8 * Appearance.factor
                        }
                        CheckBox {
                            objectName: "checkLeaksBox"
                            text: qsTr("Check for leaked passwords and breached websites")
                            checked: settingsWindow.checkLeaks
                            onToggled: settingsWindow.checkLeaks = checked
                            Layout.topMargin: 8 * Appearance.factor
                        }
                        Label {
                            text: qsTr("With Have I Been Pwned. Of each password only the first 5 characters of its hash are sent, never the password itself. The list of breached websites is fetched whole.")
                            wrapMode: Text.Wrap
                            opacity: 0.7
                            leftPadding: 28 * Appearance.factor
                            Layout.fillWidth: true
                        }
                        Item { Layout.fillHeight: true }
                    }

                    // Vaults (ref, D-057): where each vault is kept and when it
                    // was last synchronized; a menu per vault with only what can
                    // be done to it now (the owner, D-060).
                    ExactColumn {
                        id: vaultsPage
                        objectName: "vaultsPage"
                        gap: 8 * Appearance.factor
                        // Vaults not found are listed so they can be forgotten.
                        onVisibleChanged: if (visible) VaultController.checkVaults()
                        Label {
                            text: qsTr("Vaults")
                            font.pointSize: Appearance.fontPointSize * 1.4
                        }
                        Repeater {
                            model: VaultController.vaults
                            delegate: Frame {
                                id: vaultRow
                                required property string modelData
                                // Read again whenever the vaults or their syncs change.
                                property var place: VaultController.vaultPlace(modelData)
                                readonly property string name: VaultController.vaultNameOf(modelData)
                                readonly property bool found: VaultController.foundVaults.indexOf(modelData) >= 0
                                readonly property bool primary: VaultController.primaryVault === modelData
                                readonly property bool unlocked: VaultController.vaultPath === modelData
                                                                 && VaultController.state === VaultController.Unlocked
                                readonly property bool canLock: unlocked
                                readonly property bool canMove: found
                                readonly property bool canForget: !primary
                                readonly property bool canDisconnect: found && place.onWebDav === true
                                readonly property bool canImport: found
                                readonly property bool canExport: found
                                Layout.fillWidth: true
                                Connections {
                                    target: VaultController
                                    function onSyncChanged() { vaultRow.place = VaultController.vaultPlace(vaultRow.modelData) }
                                    function onVaultsChanged() { vaultRow.place = VaultController.vaultPlace(vaultRow.modelData) }
                                }
                                ExactRow {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    gap: 8 * Appearance.factor
                                    ExactColumn {
                                        gap: 2 * Appearance.factor
                                        Layout.fillWidth: true
                                        ExactRow {
                                            gap: 6 * Appearance.factor
                                            Layout.fillWidth: true
                                            Label {
                                                text: vaultRow.name
                                                textFormat: Text.PlainText
                                                font.bold: true
                                                font.strikeout: !vaultRow.found
                                                elide: Text.ElideRight
                                                Layout.fillWidth: !vaultRow.primary
                                                Layout.maximumWidth: implicitWidth
                                            }
                                            // The primary vault: a gold crown after its name (D-062).
                                            Image {
                                                objectName: "primaryCrown_" + vaultRow.name
                                                visible: vaultRow.primary
                                                source: "qrc:/icons/crown.svg"
                                                sourceSize: Qt.size(Math.round(18 * Appearance.factor), Math.round(18 * Appearance.factor))
                                                Layout.preferredWidth: Math.round(18 * Appearance.factor)
                                                Layout.preferredHeight: Math.round(18 * Appearance.factor)
                                                HoverHandler { id: overCrown }
                                                ToolTip.visible: overCrown.hovered
                                                ToolTip.text: qsTr("Primary vault")
                                            }
                                            Item { Layout.fillWidth: true }
                                        }
                                        Label {
                                            text: vaultRow.place.onWebDav ? qsTr("WebDAV: %1").arg(vaultRow.place.address)
                                                                          : qsTr("This computer: %1").arg(vaultRow.place.address)
                                            textFormat: Text.PlainText
                                            opacity: 0.7
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            visible: vaultRow.found && vaultRow.place.onWebDav === true
                                            text: qsTr("Last synchronized: %1").arg(vaultRow.place.lastSynced || "")
                                            opacity: 0.7
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            objectName: "notFound_" + vaultRow.name
                                            visible: !vaultRow.found
                                            text: qsTr("Not found")
                                            color: "#e53e3e"
                                            Layout.fillWidth: true
                                        }
                                    }
                                    ToolButton {
                                        objectName: "vaultMenuButton_" + vaultRow.name
                                        text: "⋮"
                                        font.pointSize: Appearance.fontPointSize * 1.5
                                        visible: vaultRow.canLock || vaultRow.canMove || vaultRow.canForget
                                                 || vaultRow.canDisconnect || vaultRow.canImport
                                                 || vaultRow.canExport
                                        Accessible.name: qsTr("Vault menu")
                                        onClicked: vaultMenu.popup(this, 0, height)
                                        Menu {
                                            id: vaultMenu
                                            objectName: "vaultMenu_" + vaultRow.name
                                            MenuItem {
                                                objectName: "lockVault_" + vaultRow.name
                                                text: qsTr("Lock")
                                                visible: vaultRow.canLock
                                                height: visible ? implicitHeight : 0
                                                onTriggered: VaultController.lockVault(vaultRow.modelData)
                                            }
                                            MenuItem {
                                                objectName: "moveVault_" + vaultRow.name
                                                text: qsTr("Move…")
                                                visible: vaultRow.canMove
                                                height: visible ? implicitHeight : 0
                                                onTriggered: {
                                                    moveChoice.vaultFile = vaultRow.modelData
                                                    moveChoice.open()
                                                }
                                            }
                                            MenuItem {
                                                objectName: "forgetVault_" + vaultRow.name
                                                text: qsTr("Forget…")
                                                visible: vaultRow.canForget
                                                height: visible ? implicitHeight : 0
                                                onTriggered: {
                                                    forgetWindow.vaultFile = vaultRow.modelData
                                                    forgetWindow.onWebDav = vaultRow.place.onWebDav === true
                                                    forgetWindow.open()
                                                }
                                            }
                                            MenuItem {
                                                objectName: "importVault_" + vaultRow.name
                                                text: qsTr("Import…")
                                                visible: vaultRow.canImport
                                                height: visible ? implicitHeight : 0
                                                onTriggered: unlockFirst.ask(vaultRow.modelData, "import")
                                            }
                                            MenuItem {
                                                objectName: "exportVault_" + vaultRow.name
                                                text: qsTr("Export…")
                                                visible: vaultRow.canExport
                                                height: visible ? implicitHeight : 0
                                                onTriggered: unlockFirst.ask(vaultRow.modelData, "export")
                                            }
                                            MenuItem {
                                                objectName: "disconnect_" + vaultRow.name
                                                text: qsTr("Disconnect…")
                                                visible: vaultRow.canDisconnect
                                                height: visible ? implicitHeight : 0
                                                onTriggered: {
                                                    disconnectWindow.vaultFile = vaultRow.modelData
                                                    disconnectWindow.open()
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        // Import… and Export… work on the vault that is unlocked: another
                        // vault is unlocked first with its master password (D-060).
                        DialogWindow {
                            id: unlockFirst
                            objectName: "unlockFirstWindow"
                            memoryName: "unlockFirst"
                            transientParent: settingsWindow
                            property string vaultFile: ""
                            property string action: ""
                            property bool waiting: false
                            property string problem: ""
                            readonly property bool ready: VaultController.vaultPath === vaultFile
                                                          && VaultController.state === VaultController.Unlocked
                            function ask(file, what) {
                                vaultFile = file
                                action = what
                                problem = ""
                                waiting = false
                                if (ready) {
                                    go()
                                    return
                                }
                                unlockFirstPassword.clear()
                                open()
                                unlockFirstPassword.forceActiveFocus()
                            }
                            function go() {
                                waiting = false
                                if (visible)
                                    accept()
                                if (action === "import")
                                    settingsWindow.importRequested()
                                else
                                    settingsWindow.exportRequested()
                            }
                            function unlockNow() {
                                if (unlockFirstPassword.text === "" || waiting)
                                    return
                                problem = ""
                                waiting = true
                                if (VaultController.vaultPath !== vaultFile)
                                    VaultController.switchToVault(vaultFile)
                                if (ready) {
                                    go() // opened with the password saved in the primary
                                    return
                                }
                                if (VaultController.vaultPath !== vaultFile) {
                                    waiting = false
                                    problem = VaultController.message
                                    return
                                }
                                if (VaultController.state === VaultController.Locked)
                                    VaultController.unlock(unlockFirstPassword.text)
                                unlockFirstPassword.clear()
                            }
                            title: qsTr("Unlock the vault")
                            standardButtons: Dialog.Cancel
                            onRejected: { waiting = false; unlockFirstPassword.clear() }
                            rightButtons: Button {
                                objectName: "unlockFirstButton"
                                text: qsTr("Unlock")
                                enabled: !unlockFirst.waiting && unlockFirstPassword.text !== ""
                                highlighted: true
                                onClicked: unlockFirst.unlockNow()
                            }
                            Connections {
                                target: VaultController
                                function onStateChanged() {
                                    if (!unlockFirst.waiting)
                                        return
                                    if (unlockFirst.ready) {
                                        unlockFirst.go()
                                    } else if (VaultController.state === VaultController.Locked) {
                                        unlockFirst.waiting = false
                                        unlockFirst.problem = VaultController.message
                                    }
                                }
                            }
                            ExactColumn {
                                gap: 8 * Appearance.factor
                                Layout.preferredWidth: 400 * Appearance.factor
                                Layout.fillWidth: true
                                Label {
                                    text: qsTr("Enter the master password of “%1”. It is then the vault shown.").arg(VaultController.vaultNameOf(unlockFirst.vaultFile))
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                PasswordField {
                                    id: unlockFirstPassword
                                    objectName: "unlockFirstPassword"
                                    Layout.fillWidth: true
                                    onAccepted: unlockFirst.unlockNow()
                                }
                                Label {
                                    objectName: "unlockFirstProblem"
                                    visible: unlockFirst.problem !== ""
                                    text: unlockFirst.problem
                                    color: "#e53e3e"
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        // Move…: where to, then the file here or the WebDAV folder (D-060).
                        StorageWindow {
                            id: moveChoice
                            objectName: "moveStorageWindow"
                            transientParent: settingsWindow
                            mode: "move"
                            property string vaultFile: ""
                            onChosen: function(storage) {
                                if (storage === "device") {
                                    movePicker.vaultFile = moveChoice.vaultFile
                                    movePicker.open()
                                } else {
                                    moveWindow.mode = "move"
                                    moveWindow.moveVault = moveChoice.vaultFile
                                    moveWindow.open()
                                }
                            }
                        }
                        FilePicker {
                            id: movePicker
                            property string vaultFile: ""
                            memoryName: "SettingsWindow.movePicker"
                            parentWindow: settingsWindow
                            title: qsTr("Where should the vault be moved to?")
                            fileMode: FileDialog.SaveFile
                            defaultSuffix: "chronovault"
                            nameFilters: [qsTr("Chrono Vault vaults (*.chronovault)")]
                            onAccepted: {
                                moveStatus.problem = ""
                                moveStatus.busy = true
                                moveStatus.vaultFile = movePicker.vaultFile
                                moveStatus.open()
                                VaultController.moveVaultTo(movePicker.vaultFile, selectedFile)
                            }
                        }
                        DialogWindow {
                            id: moveStatus
                            objectName: "moveStatusWindow"
                            memoryName: "moveVault2"
                            transientParent: settingsWindow
                            property string vaultFile: ""
                            property bool busy: false
                            property string problem: ""
                            title: qsTr("Move the vault")
                            standardButtons: Dialog.Close
                            Connections {
                                target: VaultController
                                function onMoveDone(ok, problem) {
                                    if (!moveStatus.busy)
                                        return
                                    moveStatus.busy = false
                                    if (ok)
                                        moveStatus.accept()
                                    else
                                        moveStatus.problem = problem
                                }
                            }
                            ExactColumn {
                                gap: 8 * Appearance.factor
                                Layout.preferredWidth: 400 * Appearance.factor
                                Label {
                                    visible: moveStatus.busy
                                    text: qsTr("Moving “%1”…").arg(VaultController.vaultNameOf(moveStatus.vaultFile))
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Label {
                                    objectName: "moveProblem"
                                    visible: moveStatus.problem !== ""
                                    text: moveStatus.problem
                                    color: "#e53e3e"
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        WebDavWindow {
                            id: moveWindow
                            objectName: "moveToWebDavWindow"
                            transientParent: settingsWindow
                        }
                        DialogWindow {
                            id: forgetWindow
                            objectName: "forgetWindow"
                            memoryName: "forgetVault2"
                            transientParent: settingsWindow
                            property string vaultFile: ""
                            property bool onWebDav: false
                            title: qsTr("Forget the vault")
                            standardButtons: Dialog.Cancel
                            rightButtons: Button {
                                objectName: "forgetButton"
                                text: qsTr("Forget")
                                onClicked: {
                                    VaultController.forgetVault(forgetWindow.vaultFile)
                                    forgetWindow.accept()
                                }
                            }
                            Label {
                                objectName: "dialogText"
                                Layout.fillWidth: true
                                text: forgetWindow.onWebDav
                                      ? qsTr("“%1” is taken off the list of vaults. Its copy on this computer is deleted at once, with any change not yet on the server. The vault on the server stays.").arg(VaultController.vaultNameOf(forgetWindow.vaultFile))
                                      : qsTr("“%1” is taken off the list of vaults. Its file stays where it is.").arg(VaultController.vaultNameOf(forgetWindow.vaultFile))
                                wrapMode: Text.Wrap
                            }
                        }
                        DialogWindow {
                            id: disconnectWindow
                            objectName: "disconnectWindow"
                            memoryName: "disconnectWebDav2"
                            transientParent: settingsWindow
                            property string vaultFile: ""
                            title: qsTr("Disconnect from WebDAV")
                            standardButtons: Dialog.Cancel
                            rightButtons: Button {
                                objectName: "disconnectButton"
                                text: qsTr("Disconnect")
                                onClicked: {
                                    VaultController.disconnectWebDav(disconnectWindow.vaultFile)
                                    disconnectWindow.accept()
                                }
                            }
                            Label {
                                objectName: "dialogText"
                                Layout.fillWidth: true
                                text: qsTr("Chrono Vault keeps a local copy of every vault that is stored in a remote location, so you can keep working when you are offline. Changes are synced automatically once your connection is restored.") + "\n"
                                      + qsTr("Disconnecting from the remote vault makes the local copy a separate individual vault. Both vaults continue to exist and nothing is deleted, but changes in one vault are no longer synced with the other vault. This action cannot be undone.") + "\n"
                                      + qsTr("Are you sure you want to disconnect “%1” from the remote vault?").arg(VaultController.vaultNameOf(disconnectWindow.vaultFile))
                                wrapMode: Text.Wrap
                            }
                        }
                    }

                    CategoriesPage {}
                    TagsPage {}
                    TemplatesPage {}

                    // Keyboard shortcuts, after Chrono Viewer's page (D-045):
                    // every action on its row with its keys one under the
                    // other, a cross after each, a + on the first line.
                    ExactColumn {
                        objectName: "shortcutsPage"
                        gap: 4 * Appearance.factor
                        // Escape cannot be set: it cancels and closes.
                        ExactRow {
                            Layout.fillWidth: true
                            gap: 6 * Appearance.factor
                            opacity: 0.5
                            Label {
                                text: qsTr("Cancel, close a window")
                                Layout.fillWidth: true
                            }
                            Label {
                                text: qsTr("Esc")
                                Layout.preferredWidth: 150 * Appearance.factor
                            }
                            // The room of the × and the +, which Escape does not have.
                            KeyButton { opacity: 0; enabled: false }
                            KeyButton { opacity: 0; enabled: false }
                        }
                        // Each action: its lines in fixed columns, as Chrono
                        // Viewer's page: the name, the key (or "no key", greyed,
                        // in the same column), the ×, and the + on the first line.
                        Repeater {
                            model: settingsWindow.keyedActions
                            delegate: ExactColumn {
                                id: shortcutRow
                                required property var modelData
                                readonly property string actionId: modelData.actionId
                                readonly property var keys: settingsWindow.keyMap[actionId] || []
                                objectName: "shortcutRow_" + actionId
                                gap: 4 * Appearance.factor
                                Layout.fillWidth: true
                                Repeater {
                                    // At least one line: "no key" when it has none.
                                    model: shortcutRow.keys.length > 0 ? shortcutRow.keys : [""]
                                    delegate: ExactRow {
                                        required property string modelData
                                        required property int index
                                        Layout.fillWidth: true
                                        gap: 6 * Appearance.factor
                                        Label {
                                            text: index === 0 ? shortcutRow.modelData.text.replace("&", "") : ""
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            objectName: modelData === "" ? "noKey" : "shortcutKey"
                                            text: modelData === "" ? qsTr("no key") : modelData
                                            opacity: modelData === "" ? 0.5 : 1
                                            horizontalAlignment: Text.AlignLeft
                                            Layout.preferredWidth: 150 * Appearance.factor
                                        }
                                        KeyButton {
                                            objectName: "removeKey_" + shortcutRow.actionId
                                            text: "×"
                                            // A line without a key has no cross; its room stays.
                                            opacity: modelData === "" ? 0 : 1
                                            enabled: modelData !== ""
                                            onClicked: settingsWindow.takeKey(shortcutRow.actionId, modelData)
                                        }
                                        KeyButton {
                                            objectName: "addKey_" + shortcutRow.actionId
                                            text: "+"
                                            opacity: index === 0 ? 1 : 0
                                            enabled: index === 0
                                            onClicked: keyCatcher.askFor(shortcutRow.actionId,
                                                                         shortcutRow.modelData.text.replace("&", ""))
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // The box that waits for a key, as Chrono Viewer's (D-053): a window
        // of its own that holds the keyboard, so every key pressed becomes the
        // new key and never runs a command. Escape cancels it; its Cancel
        // never takes the focus, so Space, Return and Tab arrive as keys.
        ApplicationWindow {
            id: keyCatcher
            objectName: "keyCatcher"
            font: Appearance.font
            Component.onCompleted: Appearance.dress(this)
            transientParent: settingsWindow
            flags: Qt.Dialog
            modality: Qt.ApplicationModal
            visible: false
            title: qsTr("Press the new key")
            color: palette.window
            width: Math.max(300 * Appearance.factor, catchLayout.implicitWidth + 2 * catchLayout.anchors.margins)
            height: catchLayout.implicitHeight + 2 * catchLayout.anchors.margins
            property string actionId: ""
            property string actionName: ""
            property string pendingKey: ""
            function askFor(id, name) {
                actionId = id
                actionName = name
                pendingKey = ""
                x = settingsWindow.x + (settingsWindow.width - width) / 2
                y = settingsWindow.y + (settingsWindow.height - height) / 2
                show()
                raise()
                requestActivate()
                catcher.forceActiveFocus()
            }
            // A key pressed, or the tests' stand-in for one.
            function take(key) {
                if (key === "")
                    return
                close()
                if ((settingsWindow.keyMap[actionId] || []).indexOf(key) >= 0)
                    return // it already answers to this one
                const holder = settingsWindow.holderOf(key, actionId)
                if (holder !== "") {
                    pendingKey = key
                    keyTaken.ask(key, settingsWindow.nameOf(holder), actionName)
                    return
                }
                settingsWindow.giveKey(actionId, key)
            }
            ExactColumn {
                id: catchLayout
                anchors.fill: parent
                anchors.margins: 12 * Appearance.factor
                gap: 12 * Appearance.factor
                Label {
                    text: qsTr("Press a key for “%1”").arg(keyCatcher.actionName)
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Item {
                    id: catcher
                    focus: true
                    implicitHeight: 1
                    Keys.onPressed: function(event) {
                        event.accepted = true
                        if (event.key === Qt.Key_Escape) {
                            keyCatcher.close()
                            return
                        }
                        keyCatcher.take(VaultController.keyName(event.key, event.modifiers))
                    }
                }
                Button {
                    text: qsTr("Cancel")
                    focusPolicy: Qt.NoFocus
                    Layout.alignment: Qt.AlignRight
                    onClicked: keyCatcher.close()
                }
            }
        }

        // A key already in use, asked as Chrono Viewer asks it: OK gives the
        // key to the new action and takes that one key from the old one;
        // Cancel, the default, changes nothing.
        ApplicationWindow {
            id: keyTaken
            objectName: "keyTaken"
            font: Appearance.font
            Component.onCompleted: Appearance.dress(this)
            transientParent: settingsWindow
            flags: Qt.Dialog
            modality: Qt.ApplicationModal
            visible: false
            title: qsTr("Keyboard shortcuts")
            color: palette.window
            width: Math.max(380 * Appearance.factor, takenLayout.implicitWidth + 2 * takenLayout.anchors.margins)
            height: takenLayout.implicitHeight + 2 * takenLayout.anchors.margins
            property string informative: ""
            function ask(key, oldName, newName) {
                informative = qsTr("%1 is assigned to “%2”. Reassign it to “%3”?").arg(key).arg(oldName).arg(newName)
                x = settingsWindow.x + (settingsWindow.width - width) / 2
                y = settingsWindow.y + (settingsWindow.height - height) / 2
                show()
                raise()
                requestActivate()
                takenCancel.forceActiveFocus()
            }
            function answer(ok) {
                if (ok)
                    settingsWindow.giveKey(keyCatcher.actionId, keyCatcher.pendingKey)
                keyCatcher.pendingKey = ""
                close()
            }
            onClosing: keyCatcher.pendingKey = ""
            ExactColumn {
                id: takenLayout
                anchors.fill: parent
                anchors.margins: 12 * Appearance.factor
                gap: 12 * Appearance.factor
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Escape) {
                        event.accepted = true
                        keyTaken.answer(false)
                    } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        event.accepted = true
                        keyTaken.answer(takenOk.activeFocus)
                    }
                }
                ExactRow {
                    gap: 12 * Appearance.factor
                    // The question mark of a question box.
                    Rectangle {
                        Layout.alignment: Qt.AlignTop
                        implicitWidth: 32 * Appearance.factor
                        implicitHeight: implicitWidth
                        radius: width / 2
                        color: keyTaken.palette.highlight
                        Label {
                            anchors.centerIn: parent
                            text: "?"
                            font.bold: true
                            font.pixelSize: parent.height * 0.6
                            color: keyTaken.palette.highlightedText
                        }
                    }
                    ExactColumn {
                        gap: 6 * Appearance.factor
                        Label {
                            text: qsTr("This key has already been assigned")
                            font.bold: true
                        }
                        Label {
                            text: keyTaken.informative
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                    }
                }
                ExactRow {
                    Layout.alignment: Qt.AlignRight
                    Button {
                        id: takenOk
                        objectName: "keyTakenOk"
                        text: qsTr("OK")
                        onClicked: keyTaken.answer(true)
                    }
                    Button {
                        id: takenCancel
                        objectName: "keyTakenCancel"
                        text: qsTr("Cancel")
                        highlighted: true
                        onClicked: keyTaken.answer(false)
                    }
                }
            }
        }

        // OK, Cancel and Restore defaults at the bottom right.
        ExactRow {
            id: bottomRow
            Layout.fillWidth: true
            gap: 6 * Appearance.factor
            // The Vaults page's own buttons, at the section's left edge (D-062).
            Item {
                visible: settingsWindow.onVaults
                Layout.preferredWidth: Math.max(0, pageScroll.x - bottomRow.gap)
            }
            Button {
                objectName: "createVaultButton"
                visible: settingsWindow.onVaults
                text: qsTr("Create vault")
                onClicked: settingsWindow.createVaultRequested()
            }
            Button {
                objectName: "openVaultButton"
                visible: settingsWindow.onVaults
                text: qsTr("Open vault")
                onClicked: settingsWindow.openVaultRequested()
            }
            Label {
                objectName: "problemLabel"
                text: settingsWindow.problem
                visible: text !== ""
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                id: helloLine
                objectName: "helloLine"
                text: VaultController.quickUnlockBusy ? qsTr("Waiting for Windows Hello fingerprint...")
                                                      : VaultController.quickUnlockMessage
                visible: settingsWindow.helloOnTheLine && settingsWindow.problem === "" && text !== ""
                color: "#e53e3e"
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Item {
                visible: settingsWindow.problem === "" && !helloLine.visible
                Layout.fillWidth: true
            }
            Button {
                objectName: "restoreDefaultsButton"
                // Nothing to put back on the Vaults page (D-062).
                visible: settingsWindow.restorable
                text: qsTr("Restore defaults")
                onClicked: settingsWindow.restoreDefaults()
            }
            Button {
                id: okButton
                objectName: "okButton"
                text: qsTr("OK")
                highlighted: true
                enabled: settingsWindow.pinAcceptable
                icon.source: "qrc:/icons/ok.svg"
                icon.width: Math.round(16 * Appearance.factor)
                icon.height: Math.round(16 * Appearance.factor)
                icon.color: settingsWindow.palette.buttonText
                onClicked: settingsWindow.accept()
            }
            Button {
                objectName: "cancelButton"
                text: qsTr("Cancel")
                icon.source: "qrc:/icons/cancel.svg"
                icon.width: Math.round(16 * Appearance.factor)
                icon.height: Math.round(16 * Appearance.factor)
                icon.color: settingsWindow.palette.buttonText
                onClicked: settingsWindow.close()
            }
        }
    }
}
