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
// SettingsWindow.qml 0.0.27

// --- Version history ----------------------------------------------------
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

    // Opened at General (the View menu).
    function showGeneral() {
        showWindow()
        topics.currentIndex = 0
    }

    // What the window shows, until OK keeps it.
    property bool keepInTray: false
    property bool startAtStartup: false
    property bool startMinimized: false
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
    property bool masterPasswordReminder: true
    // Shown at once while the window is open; kept with OK, put back otherwise.
    property int theme: 2
    property int scale: 100

    function loadSettings() {
        theme = Appearance.theme
        scale = Appearance.scale
        keepInTray = VaultController.keepInTray
        startAtStartup = VaultController.startAtStartup
        startMinimized = VaultController.startMinimized
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

    // What went wrong at the last OK, shown beside the buttons.
    property string problem: ""
    // Closed without OK (Cancel, Esc, the window's own close): the theme and
    // the scale it showed are put back to what is kept.
    onVisibleChanged: {
        if (!visible) {
            problem = ""
            // A finger still asked for turning it on: the reader is given back.
            VaultController.cancelQuickUnlockSetup()
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
                    objectName: "topics"
                    anchors.fill: parent
                    anchors.margins: parent.border.width
                    clip: true
                    currentIndex: 0
                    model: [qsTr("General"), qsTr("Security"), qsTr("Clipboard"), qsTr("Customize")]
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
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true

                StackLayout {
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
                        // Autolock when (D-040).
                        Label {
                            text: qsTr("Autolock when")
                            font.bold: true
                            Layout.topMargin: Math.round(8 * Appearance.factor)
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: qsTr("Main window is closed")
                                Layout.fillWidth: true
                            }
                            Switch {
                                objectName: "lockWhenClosedSwitch"
                                text: checked ? qsTr("On") : qsTr("Off")
                                LayoutMirroring.enabled: true
                                checked: settingsWindow.lockWhenClosed
                                onToggled: settingsWindow.lockWhenClosed = checked
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: qsTr("System sleeps")
                                Layout.fillWidth: true
                            }
                            Switch {
                                objectName: "lockOnSleepSwitch"
                                text: checked ? qsTr("On") : qsTr("Off")
                                LayoutMirroring.enabled: true
                                checked: settingsWindow.lockOnSleep
                                onToggled: settingsWindow.lockOnSleep = checked
                            }
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
                        CheckBox {
                            objectName: "hideSensitiveBox"
                            text: qsTr("Hide sensitive fields: show them as dots until asked")
                            checked: settingsWindow.hideSensitive
                            onToggled: settingsWindow.hideSensitive = checked
                            Layout.topMargin: Math.round(8 * Appearance.factor)
                        }
                        // Unlocking with a fingerprint (D-038): for the open
                        // vault, at once, not with OK.
                        Label {
                            text: qsTr("Unlock with %1").arg(VaultController.quickUnlockName || qsTr("fingerprint"))
                            font.bold: true
                            Layout.topMargin: Math.round(12 * Appearance.factor)
                        }
                        Label {
                            text: !VaultController.quickUnlockAvailable
                                  ? (Qt.platform.os === "windows"
                                     ? qsTr("Windows Hello is not set up on this computer.")
                                     : qsTr("No fingerprint reader is set up on this computer."))
                                  : VaultController.state !== VaultController.Unlocked
                                  ? qsTr("Unlock a vault to turn this on for it.")
                                  : VaultController.quickUnlockOn
                                  ? qsTr("On for this vault, on this computer.")
                                  : qsTr("Off for this vault. Turning it on asks for your finger once.")
                            wrapMode: Text.Wrap
                            opacity: 0.8
                            Layout.fillWidth: true
                        }
                        // A switch, at once, not with OK (D-042): on asks for
                        // one touch of the reader to make sure it works.
                        RowLayout {
                            Layout.fillWidth: true
                            Item { Layout.fillWidth: true }
                            Switch {
                                id: quickUnlockSwitch
                                objectName: "quickUnlockToggle"
                                text: checked ? qsTr("On") : qsTr("Off")
                                LayoutMirroring.enabled: true
                                checked: VaultController.quickUnlockOn || VaultController.quickUnlockBusy
                                enabled: VaultController.quickUnlockAvailable && !VaultController.quickUnlockBusy
                                         && VaultController.state === VaultController.Unlocked
                                onToggled: {
                                    VaultController.setQuickUnlock(checked)
                                    checked = Qt.binding(function() {
                                        return VaultController.quickUnlockOn || VaultController.quickUnlockBusy
                                    })
                                }
                            }
                        }
                        // "Touch the fingerprint reader…" and what came of it,
                        // here where it was asked (D-042).
                        RowLayout {
                            visible: VaultController.quickUnlockMessage !== "" || VaultController.quickUnlockBusy
                            Layout.fillWidth: true
                            Label {
                                objectName: "quickUnlockMessage"
                                text: VaultController.quickUnlockMessage
                                font.bold: VaultController.quickUnlockTrouble
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            Button {
                                objectName: "quickUnlockCancel"
                                visible: VaultController.quickUnlockBusy
                                text: qsTr("Cancel")
                                onClicked: VaultController.cancelQuickUnlockSetup()
                            }
                        }
                        CheckBox {
                            objectName: "reminderBox"
                            text: qsTr("Ask for the master password once a year, so it is not forgotten")
                            checked: settingsWindow.masterPasswordReminder
                            onToggled: settingsWindow.masterPasswordReminder = checked
                        }
                        Item { Layout.fillHeight: true }
                    }

                    ColumnLayout {
                        spacing: Math.round(8 * Appearance.factor)
                        Label {
                            text: qsTr("Clipboard")
                            font.pointSize: Appearance.fontPointSize * 1.4
                        }
                        Label {
                            text: qsTr("Clear what was copied after this many seconds (0 means never):")
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        SpinBox {
                            objectName: "clipboardBox"
                            from: 0
                            to: 600
                            value: settingsWindow.clipboardClearSeconds
                            editable: true
                            onValueModified: settingsWindow.clipboardClearSeconds = value
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
