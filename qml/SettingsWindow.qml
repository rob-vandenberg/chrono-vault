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
import QtQuick.Layouts

// --- Version ------------------------------------------------------------
// SettingsWindow.qml 0.0.40

// --- Version history ----------------------------------------------------
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
    // The whole window, title bar and frame included, is the size set here
    // (the owner): the frame the desktop draws, known once it is shown, is
    // taken off the inside.
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

    // Help > Keyboard shortcuts… (D-045).
    function showShortcuts() {
        showWindow()
        topics.currentIndex = 4
    }

    // The small square buttons of the Keyboard shortcuts page: × and +, a grey
    // fill, as on Chrono Viewer's page (D-047).
    component KeyButton: Button {
        implicitWidth: Math.round(24 * Appearance.factor)
        implicitHeight: Math.round(24 * Appearance.factor)
        padding: 0
        font.pixelSize: Math.round(14 * Appearance.factor)
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
    property int clipboardClearSeconds: 0
    property bool hideSensitive: true
    property bool showSubtitles: true
    property bool useWebsiteIcons: false
    property int masterPasswordReminder: 2
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
        masterPasswordReminder = VaultController.masterPasswordReminder
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
    Text {
        id: widestPercent
        visible: false
        text: qsTr("%1%").arg(Appearance.largestScale)
        font: Appearance.font
    }

    // OK: every topic's changes are kept, whichever topic is shown.
    function accept() {
        // A number still being typed counts.
        okButton.forceActiveFocus()
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

    // Ticking the fingerprint: "Touch the fingerprint reader…" with Cancel,
    // then what came of it, in a popup (the owner, D-043).
    Popup {
        objectName: "quickUnlockPopup"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.NoAutoClose
        visible: settingsWindow.visible && (VaultController.quickUnlockBusy || VaultController.quickUnlockMessage !== "")
        padding: Math.round(16 * Appearance.factor)
        contentItem: ColumnLayout {
            spacing: Math.round(12 * Appearance.factor)
            Label {
                objectName: "quickUnlockMessage"
                text: VaultController.quickUnlockMessage
                font.bold: VaultController.quickUnlockTrouble
                wrapMode: Text.Wrap
                Layout.maximumWidth: Math.round(360 * Appearance.factor)
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
            masterPasswordReminder = defaults.masterPasswordReminder
            break
        case 2:
            clipboardClearSeconds = defaults.clipboardClearSeconds
            break
        case 3:
            showSubtitles = defaults.showSubtitles
            useWebsiteIcons = defaults.useWebsiteIcons
            break
        case 4: {
            const map = {}
            for (const action of keyedActions)
                map[action.actionId] = VaultController.defaultKeysOf(action.actionId)
            keyMap = map
            break
        }
        }
    }

    title: qsTr("Settings – Chrono Vault %1").arg(VaultController.version)
    width: Math.min(Math.round(730 * Appearance.factor), Appearance.screenRoom(this).width) - frame.width
    height: Math.min(Math.round(650 * Appearance.factor), Appearance.screenRoom(this).height) - frame.height
    // Never smaller than it opens (the owner).
    minimumWidth: Math.min(Math.round(730 * Appearance.factor), Appearance.screenRoom(this).width) - frame.width
    minimumHeight: Math.min(Math.round(650 * Appearance.factor), Appearance.screenRoom(this).height) - frame.height
    visible: false
    color: palette.window

    WindowMemory {
        window: settingsWindow
        name: "settings"
    }

    // Esc is Cancel, unless the list of "Check for updates" is open: then it
    // closes only the list.
    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: !frequencyBox.popup.visible
        onActivated: settingsWindow.close()
    }
    // OK is the default button: Enter keeps the changes, unless a button has
    // the keyboard focus (then Enter presses that button) or the list of
    // "Check for updates" is open (then Enter chooses in it).
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: !frequencyBox.popup.visible
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
            Text {
                required property string modelData
                text: modelData
                font: Appearance.font
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Math.round(10 * Appearance.factor)
        spacing: Math.round(10 * Appearance.factor)

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Math.round(12 * Appearance.factor)

            // The topics: a framed panel, the chosen one marked across its
            // whole width.
            Rectangle {
                objectName: "topicsPanel"
                Layout.fillHeight: true
                Layout.preferredWidth: Math.ceil(topicNames.implicitWidth) + 2 * topicPadding + 2 * border.width
                readonly property int topicPadding: 6
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
                    model: [qsTr("General"), qsTr("Security"), qsTr("Clipboard"), qsTr("Customize"),
                            qsTr("Keyboard shortcuts")]
                    delegate: Rectangle {
                        id: topic
                        required property int index
                        required property string modelData
                        readonly property bool chosen: ListView.isCurrentItem
                        width: ListView.view.width
                        height: topicText.implicitHeight + Math.round(6 * Appearance.factor)
                        color: chosen ? settingsWindow.palette.highlight : "transparent"
                        Text {
                            id: topicText
                            anchors.verticalCenter: parent.verticalCenter
                            x: 6
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
                rightPadding: pageScrollBar.width + Math.round(6 * Appearance.factor)
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
                               ? pageStack.children[pageStack.currentIndex].implicitHeight + Math.round(10 * Appearance.factor)
                               : 0
                clip: true

                StackLayout {
                    id: pageStack
                    currentIndex: topics.currentIndex
                    width: pageScroll.availableWidth

                    ColumnLayout {
                        spacing: Math.round(8 * Appearance.factor)
                        Label {
                            text: qsTr("General")
                            font.pointSize: Appearance.fontPointSize * 1.4
                        }
                        Label { text: qsTr("Theme") }
                        Frame {
                            Layout.fillWidth: true
                            ColumnLayout {
                                spacing: 0
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
                        RowLayout {
                            objectName: "scaleRow"
                            Layout.fillWidth: true
                            // Half a line height of room above and below (the owner).
                            Layout.topMargin: Math.round(widestPercent.implicitHeight / 2)
                            Layout.bottomMargin: Math.round(widestPercent.implicitHeight / 2)
                            spacing: Math.round(12 * Appearance.factor)
                            Label { text: qsTr("UI scaling") }
                            Item {
                                id: scaleTrack
                                readonly property int markHeight: Math.round(5 * Appearance.factor)
                                Layout.fillWidth: true
                                // The marks on top, the slider below them.
                                implicitHeight: markHeight + Math.round(2 * Appearance.factor) + scaleSlider.implicitHeight
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
                        RowLayout {
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

                    ColumnLayout {
                        spacing: Math.round(8 * Appearance.factor)
                        Label {
                            text: qsTr("Security")
                            font.pointSize: Appearance.fontPointSize * 1.4
                        }
                        // Change the Master Password, as a row of its own
                        // (the reference program's settings).
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: qsTr("Change the Master Password")
                                Layout.fillWidth: true
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
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("Ask for the master password, so it is not forgotten, every") }
                            ComboBox {
                                objectName: "reminderBox"
                                model: [qsTr("Day"), qsTr("Month"), qsTr("Quarter"), qsTr("Year"), qsTr("Never")]
                                currentIndex: settingsWindow.masterPasswordReminder
                                onActivated: function(index) { settingsWindow.masterPasswordReminder = index }
                            }
                        }
                        // Autolock when (D-040): four tick boxes in a frame of
                        // their own, the title on its border (D-049).
                        GroupBox {
                            objectName: "autolockGroup"
                            title: qsTr("Autolock when")
                            Layout.fillWidth: true
                            Layout.topMargin: Math.round(8 * Appearance.factor)
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: Math.round(8 * Appearance.factor)
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
                                RowLayout {
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
                                    }
                                    Label { text: qsTr("Minutes") }
                                }
                                RowLayout {
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
                        CheckBox {
                            objectName: "hideSensitiveBox"
                            text: qsTr("Hide sensitive fields: show them as dots until asked")
                            checked: settingsWindow.hideSensitive
                            onToggled: settingsWindow.hideSensitive = checked
                            Layout.topMargin: Math.round(8 * Appearance.factor)
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
                            Layout.topMargin: Math.round(8 * Appearance.factor)
                            onToggled: {
                                VaultController.setQuickUnlock(checked)
                                checked = Qt.binding(function() {
                                    return VaultController.quickUnlockEnabled || VaultController.quickUnlockBusy
                                })
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }

                    ColumnLayout {
                        spacing: Math.round(8 * Appearance.factor)
                        Label {
                            text: qsTr("Clipboard")
                            font.pointSize: Appearance.fontPointSize * 1.4
                        }
                        // One line; 0 shows as "Never" (D-049).
                        RowLayout {
                            Label { text: qsTr("Clear the clipboard after") }
                            SpinBox {
                                objectName: "clipboardBox"
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
                        Item { Layout.fillHeight: true }
                    }

                    // Kept in the vault at once; Restore defaults leaves it as
                    // it is (D-027).
                    ColumnLayout {
                        spacing: Math.round(8 * Appearance.factor)
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
                            text: qsTr("Each website in your vault is asked for its own icon, which that website can then see. The icons are kept only while the vault is unlocked, never on the disk.")
                            wrapMode: Text.Wrap
                            opacity: 0.7
                            leftPadding: Math.round(28 * Appearance.factor)
                            Layout.fillWidth: true
                        }
                        CustomizePage { Layout.fillWidth: true }
                    }

                    // Keyboard shortcuts, after Chrono Viewer's page (D-045):
                    // every action on its row with its keys one under the
                    // other, a cross after each, a + on the first line.
                    ColumnLayout {
                        objectName: "shortcutsPage"
                        spacing: Math.round(4 * Appearance.factor)
                        // Escape cannot be set: it cancels and closes.
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Math.round(6 * Appearance.factor)
                            opacity: 0.5
                            Label {
                                text: qsTr("Cancel, close a window")
                                Layout.fillWidth: true
                            }
                            Label {
                                text: qsTr("Esc")
                                Layout.preferredWidth: Math.round(150 * Appearance.factor)
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
                            delegate: ColumnLayout {
                                id: shortcutRow
                                required property var modelData
                                readonly property string actionId: modelData.actionId
                                readonly property var keys: settingsWindow.keyMap[actionId] || []
                                objectName: "shortcutRow_" + actionId
                                spacing: Math.round(4 * Appearance.factor)
                                Layout.fillWidth: true
                                Repeater {
                                    // At least one line: "no key" when it has none.
                                    model: shortcutRow.keys.length > 0 ? shortcutRow.keys : [""]
                                    delegate: RowLayout {
                                        required property string modelData
                                        required property int index
                                        Layout.fillWidth: true
                                        spacing: Math.round(6 * Appearance.factor)
                                        Label {
                                            text: index === 0 ? shortcutRow.modelData.text.replace("&", "") : ""
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            objectName: modelData === "" ? "noKey" : "shortcutKey"
                                            text: modelData === "" ? qsTr("no key") : modelData
                                            opacity: modelData === "" ? 0.5 : 1
                                            horizontalAlignment: Text.AlignLeft
                                            Layout.preferredWidth: Math.round(150 * Appearance.factor)
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

        // The box that waits for a key: pressed, not typed (D-045). Escape
        // cancels it.
        Popup {
            id: keyCatcher
            objectName: "keyCatcher"
            parent: Overlay.overlay
            anchors.centerIn: parent
            modal: true
            focus: true
            closePolicy: Popup.NoAutoClose
            padding: Math.round(16 * Appearance.factor)
            property string actionId: ""
            property string actionName: ""
            property string pendingKey: ""
            function askFor(id, name) {
                actionId = id
                actionName = name
                pendingKey = ""
                open()
                catcher.forceActiveFocus()
            }
            // A key pressed, or the tests' stand-in for one.
            function take(key) {
                if (key === "")
                    return
                if (settingsWindow.holderOf(key, actionId) !== "") {
                    pendingKey = key // asked first: already assigned
                    return
                }
                settingsWindow.giveKey(actionId, key)
                close()
            }
            contentItem: ColumnLayout {
                spacing: Math.round(12 * Appearance.factor)
                Label {
                    text: keyCatcher.pendingKey === ""
                          ? qsTr("Press the new key for “%1”.").arg(keyCatcher.actionName)
                          : qsTr("This key has already been assigned")
                    font.bold: keyCatcher.pendingKey !== ""
                }
                Item {
                    id: catcher
                    focus: true
                    implicitHeight: 1
                    Keys.onPressed: function(event) {
                        event.accepted = true
                        if (keyCatcher.pendingKey !== "")
                            return
                        if (event.key === Qt.Key_Escape) {
                            keyCatcher.close()
                            return
                        }
                        keyCatcher.take(VaultController.keyName(event.key, event.modifiers))
                    }
                }
                RowLayout {
                    visible: keyCatcher.pendingKey !== ""
                    Layout.alignment: Qt.AlignRight
                    Button {
                        objectName: "keyTakenOk"
                        text: qsTr("OK")
                        onClicked: {
                            settingsWindow.giveKey(keyCatcher.actionId, keyCatcher.pendingKey)
                            keyCatcher.close()
                        }
                    }
                    Button {
                        objectName: "keyTakenCancel"
                        text: qsTr("Cancel")
                        onClicked: keyCatcher.close()
                    }
                }
                Button {
                    visible: keyCatcher.pendingKey === ""
                    text: qsTr("Cancel")
                    Layout.alignment: Qt.AlignRight
                    onClicked: keyCatcher.close()
                }
            }
        }

        // OK, Cancel and Restore defaults at the bottom right.
        RowLayout {
            Layout.fillWidth: true
            spacing: Math.round(6 * Appearance.factor)
            Label {
                objectName: "problemLabel"
                text: settingsWindow.problem
                visible: text !== ""
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Item {
                visible: settingsWindow.problem === ""
                Layout.fillWidth: true
            }
            Button {
                objectName: "restoreDefaultsButton"
                text: qsTr("Restore defaults")
                onClicked: settingsWindow.restoreDefaults()
            }
            Button {
                id: okButton
                objectName: "okButton"
                text: qsTr("OK")
                highlighted: true
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
