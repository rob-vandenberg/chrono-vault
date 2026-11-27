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
import QtQuick.Controls.impl
import QtQuick.Dialogs
import QtQuick.Layouts

// --- Version ------------------------------------------------------------
// SettingsWindow.qml 0.8.126

// --- Version history ----------------------------------------------------
// v0.8.126: The standard master password dialog, the primary vault's, before the finger (D-043)
// v0.8.125: Ticking the fingerprint box first asks the open vault's master password (D-043)
// v0.8.122: Delete… in every vault's menu: the primary, open or closed say why not; a locked vault asks its password (D-083)
// v0.8.121: Buttons alike only inside a group, 28 high; the key question's first line bold (D-082)
// v0.8.120: The edge of every dialog, its buttons alike; the key dialogs and the paragraphs in the one design (D-082)
// v0.8.119: Disconnect deletes nothing, its new text (D-060); no minimize button (D-079); Check for updates above the clipboard
// v0.8.118: A double click on a vault's row opens it, as Open in its menu (D-071)
// v0.8.117: A cloud vault's line shows its account; the line was empty (arg takes one value in QML) (D-080 point 20)
// v0.8.115: Any storage in the vault lines and Disconnect; Move to Google Drive; Also delete the vault on Google Drive (D-080)
// v0.7.111: A vault row shows where it is kept; question dialogs fit their text (D-079)
// v0.7.110: Open with what is kept: the vault shown stays until the other one is open (D-078)
// v0.7.107: Set PIN…, Change PIN… and Remove PIN… in a vault's menu; the PIN line leaves Security (D-076)
// v0.6.106: Close and Lock per vault; the primary first; back to the vault after a lock (D-075)
// v0.6.105: Another primary vault; Remember my credentials; Open, Closed, Locked (D-074)
// v0.6.103: One standard dialog for a master password, a PIN or a finger (D-073)
// v0.6.102: An outdated kept password is said in Settings; the gear is "Options" (D-071)
// v0.6.100: Padlocks and Open in Settings > Vaults; round picture buttons (D-071)
// v0.6.99: The "?" of a key in use is scaled once (D-069)
// v0.6.98: The texts of Forget (the owner)
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
    // Open of a vault's menu, once an edit has been asked about (D-071): at once
    // with the password kept in the primary, else its master password is asked.
    signal openKnownRequested(string vaultFile)
    signal closeVaultRequested(string vaultFile)
    Connections {
        target: VaultController
        function onVaultMenuProblem(text) { settingsWindow.problem = text }
    }
    // Delete… (D-083): the checks in the owner's order, then the question.
    function askDelete(vaultFile) {
        settingsWindow.problem = ""
        const refusal = VaultController.deleteRefusal(vaultFile)
        if (refusal !== 0) {
            noDeleteWindow.vaultFile = vaultFile
            noDeleteWindow.refusal = refusal
            noDeleteWindow.open()
        } else {
            deleteVaultWindow.askVault(vaultFile)
        }
    }
    function openKnown(vaultFile) {
        problem = ""
        unlockFirst.ask(vaultFile, "open")
    }
    // With the kept key or password; then what it was opened for.
    property string openingFor: ""
    function openKept(vaultFile, what) {
        problem = ""
        opening = vaultFile
        openingFor = what
        VaultController.switchToVault(vaultFile)
        if (VaultController.vaultPath !== vaultFile && !VaultController.switching) {
            opening = ""
            problem = VaultController.message
        }
    }
    // The vault being opened with a kept password: when that no longer opens it,
    // the reason is said here too (D-071).
    property string opening: ""
    Connections {
        target: VaultController
        enabled: settingsWindow.opening !== ""
        function onStateChanged() {
            if (VaultController.state !== VaultController.Unlocked)
                return
            settingsWindow.opening = ""
            if (settingsWindow.openingFor === "import")
                settingsWindow.importRequested()
            else if (settingsWindow.openingFor === "export")
                settingsWindow.exportRequested()
        }
        function onMessageChanged() {
            if (VaultController.state !== VaultController.Locked || VaultController.message === "")
                return
            if (VaultController.vaultPath === settingsWindow.opening)
                settingsWindow.problem = VaultController.message
            settingsWindow.opening = ""
        }
        // The vault shown stayed: the reason is said here (D-078).
        function onSwitchFailed(vaultFile) {
            if (vaultFile === settingsWindow.opening)
                settingsWindow.problem = VaultController.message
            settingsWindow.opening = ""
        }
    }
    // While another vault opens, nothing here reaches the vault shown (D-078).
    MouseArea {
        objectName: "settingsSwitchingShield"
        anchors.fill: parent
        z: 1000
        visible: VaultController.switching
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: function(wheel) { wheel.accepted = true }
        Keys.onPressed: function(event) { event.accepted = true }
        onVisibleChanged: if (visible) forceActiveFocus()
    }
    signal exportRequested()
    property int clipboardClearSeconds: 0
    property bool hideSensitive: true
    property bool showSubtitles: true
    property bool useWebsiteIcons: false
    property bool checkLeaks: false
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
        checkLeaks = VaultController.checkLeaks
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
    Label {
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
        // The edge of every dialog, on all four sides (D-082).
        anchors.margins: 12 * Appearance.factor
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
                        // Above the clipboard's line (the owner, 0.8.119).
                        ExactRow {
                            // As far apart as the tick boxes, this line and the next (the owner).
                            Layout.topMargin: 9 * Appearance.factor
                            Label { text: qsTr("Check for updates") }
                            ComboBox {
                                id: frequencyBox
                                objectName: "checkFrequencyBox"
                                model: [qsTr("Daily"), qsTr("Weekly"), qsTr("Monthly"), qsTr("Never")]
                                currentIndex: settingsWindow.checkFrequency
                                onActivated: function(index) { settingsWindow.checkFrequency = index }
                            }
                        }
                        // One line; 0 shows as "Never" (D-049).
                        ExactRow {
                            Layout.topMargin: 5 * Appearance.factor
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
                        // One line, for all vaults (the owner, D-043): ticked, it
                        // first asks the master password, typed, and only
                        // then one touch in a popup, at once, not with OK.
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
                                if (checked)
                                    fingerPasswordWindow.askVault(VaultController.primaryVault)
                                else
                                    VaultController.setQuickUnlock(false)
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
                                // Opens without a password (D-075).
                                readonly property bool kept: VaultController.keptRevision >= 0 && VaultController.vaults.length >= 0
                                                             && VaultController.isKept(modelData)
                                readonly property bool closed: VaultController.keptRevision >= 0 && !unlocked
                                                               && VaultController.vaults.length >= 0
                                                               && VaultController.vaultStateOf(modelData) === 1
                                readonly property bool canClose: unlocked && kept
                                readonly property bool canLock: unlocked ? !kept : closed
                                readonly property bool canOpen: found && !unlocked
                                readonly property bool canMove: found
                                readonly property bool canForget: !primary
                                readonly property bool canDisconnect: found && place.inCloud === true
                                readonly property bool canImport: found
                                readonly property bool canExport: found
                                Layout.fillWidth: true
                                TapHandler {
                                    acceptedButtons: Qt.RightButton
                                    onTapped: vaultMenu.popup()
                                }
                                // A double click opens the vault, as Open in its menu (the owner,
                                // 27 November 2026); never on the three dots.
                                TapHandler {
                                    objectName: "vaultRowOpener_" + vaultRow.name
                                    acceptedButtons: Qt.LeftButton
                                    onDoubleTapped: function(eventPoint) {
                                        const onDots = vaultMenuButton.contains(vaultMenuButton.mapFromItem(vaultRow, eventPoint.position))
                                        if (vaultRow.canOpen && !onDots)
                                            settingsWindow.openKnownRequested(vaultRow.modelData)
                                    }
                                }
                                Connections {
                                    target: VaultController
                                    function onSyncChanged() { vaultRow.place = VaultController.vaultPlace(vaultRow.modelData) }
                                    function onVaultsChanged() { vaultRow.place = VaultController.vaultPlace(vaultRow.modelData) }
                                }
                                ExactRow {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    gap: 8 * Appearance.factor
                                    // Where it is kept (the owner, D-079).
                                    IconImage {
                                        source: vaultRow.place.placePicture
                                        color: palette.windowText
                                        sourceSize.width: Math.round(32 * Appearance.factor)
                                        sourceSize.height: Math.round(32 * Appearance.factor)
                                        Layout.preferredWidth: Math.round(32 * Appearance.factor)
                                        Layout.preferredHeight: Math.round(32 * Appearance.factor)
                                        Layout.alignment: Qt.AlignVCenter
                                    }
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
                                                Layout.fillWidth: true
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
                                            // Open or locked, seen by shape and color (the owner, D-071).
                                            Padlock {
                                                objectName: "padlock_" + vaultRow.name
                                                vaultFile: vaultRow.modelData
                                                paper: settingsWindow.palette.window
                                                ink: settingsWindow.palette.windowText
                                                visible: vaultRow.found
                                                Layout.preferredWidth: Math.round(wide)
                                                Layout.preferredHeight: Math.round(18 * Appearance.factor)
                                            }
                                            Item { Layout.fillWidth: true }
                                        }
                                        Label {
                                            objectName: "placeLine_" + vaultRow.name
                                            // A cloud vault's account (the owner, 27 November 2026); arg()
                                            // takes one value in QML, two make the line empty.
                                            text: !vaultRow.place.inCloud ? qsTr("This computer: %1").arg(vaultRow.place.address)
                                                  : vaultRow.place.account ? qsTr("%1: %2").arg(vaultRow.place.storageName).arg(vaultRow.place.account)
                                                  : vaultRow.place.storageName
                                            textFormat: Text.PlainText
                                            opacity: 0.7
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            visible: vaultRow.found && vaultRow.place.inCloud === true
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
                                    PictureButton {
                                        id: vaultMenuButton
                                        objectName: "vaultMenuButton_" + vaultRow.name
                                        dots: true
                                        menuOpen: vaultMenu.visible
                                        // Always: Delete… is in every vault's menu (D-083).
                                        Accessible.name: qsTr("Vault menu")
                                        onClicked: vaultMenu.popup(this, 0, height)
                                        Menu {
                                            id: vaultMenu
                                            objectName: "vaultMenu_" + vaultRow.name
                                            // Open, Close and Lock, by the vault's state (the owner, D-071, D-075).
                                            MenuItem {
                                                objectName: "openVault_" + vaultRow.name
                                                text: qsTr("Open")
                                                visible: vaultRow.canOpen
                                                height: visible ? implicitHeight : 0
                                                onTriggered: settingsWindow.openKnownRequested(vaultRow.modelData)
                                            }
                                            MenuItem {
                                                objectName: "closeVault_" + vaultRow.name
                                                text: qsTr("Close")
                                                visible: vaultRow.canClose
                                                height: visible ? implicitHeight : 0
                                                onTriggered: {
                                                    settingsWindow.problem = ""
                                                    settingsWindow.closeVaultRequested(vaultRow.modelData)
                                                }
                                            }
                                            MenuItem {
                                                objectName: "lockVault_" + vaultRow.name
                                                text: qsTr("Lock")
                                                visible: vaultRow.canLock
                                                height: visible ? implicitHeight : 0
                                                onTriggered: {
                                                    settingsWindow.problem = ""
                                                    VaultController.lockVault(vaultRow.modelData)
                                                }
                                            }
                                            // A PIN of its own (the owner, D-076).
                                            MenuItem {
                                                objectName: "setPin_" + vaultRow.name
                                                readonly property bool has: VaultController.pinRevision >= 0
                                                                            && VaultController.hasPin(vaultRow.modelData)
                                                text: has ? qsTr("Change PIN…") : qsTr("Set PIN…")
                                                visible: vaultRow.found
                                                height: visible ? implicitHeight : 0
                                                onTriggered: {
                                                    settingsWindow.problem = ""
                                                    pinWindow.title = has ? qsTr("Change PIN") : qsTr("Set PIN")
                                                    pinWindow.askVault(vaultRow.modelData)
                                                }
                                            }
                                            MenuItem {
                                                objectName: "removePin_" + vaultRow.name
                                                visible: vaultRow.found && VaultController.pinRevision >= 0
                                                         && VaultController.hasPin(vaultRow.modelData)
                                                text: qsTr("Remove PIN…")
                                                height: visible ? implicitHeight : 0
                                                onTriggered: {
                                                    removePinWindow.vaultFile = vaultRow.modelData
                                                    removePinWindow.open()
                                                }
                                            }
                                            MenuItem {
                                                objectName: "primaryVault_" + vaultRow.name
                                                text: qsTr("Make primary vault")
                                                visible: vaultRow.found && !vaultRow.primary
                                                height: visible ? implicitHeight : 0
                                                onTriggered: {
                                                    primaryWindow.vaultFile = vaultRow.modelData
                                                    primaryWindow.open()
                                                }
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
                                                    forgetWindow.onWebDav = vaultRow.place.inCloud === true
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
                                                    disconnectWindow.storageName = vaultRow.place.storageName || ""
                                                    disconnectWindow.open()
                                                }
                                            }
                                            // Every vault, every state (the owner, D-083).
                                            MenuItem {
                                                objectName: "deleteVault_" + vaultRow.name
                                                text: qsTr("Delete…")
                                                onTriggered: settingsWindow.askDelete(vaultRow.modelData)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        // Import… and Export… work on the vault that is unlocked: another
                        // vault is unlocked first with its master password (D-060).
                        AskWindow {
                            id: unlockFirst
                            objectName: "unlockFirstWindow"
                            transientParent: settingsWindow
                            withList: true
                            mayRemember: true
                            property string action: ""
                            function ready(file) {
                                return VaultController.vaultPath === file && VaultController.state === VaultController.Unlocked
                            }
                            // A vault that needs no password: done at once, without asking.
                            function ask(file, what) {
                                action = what
                                if (ready(file)) {
                                    goOn()
                                } else if (VaultController.opensAtOnce(file)) {
                                    if (visible)
                                        reject()
                                    settingsWindow.openKept(file, what)
                                } else {
                                    askVault(file)
                                }
                            }
                            function goOn() {
                                if (visible)
                                    accept()
                                if (action === "import")
                                    settingsWindow.importRequested()
                                else if (action === "export")
                                    settingsWindow.exportRequested()
                            }
                            onPicked: function(file) { ask(file, action) }
                            // Right: the vault is opened with it, and only now the other one closes.
                            property string opened: ""
                            onRight: function(file) {
                                opened = file
                                if (!VaultController.openProven(remembers))
                                    opened = ""
                                else if (ready(file))
                                    finish()
                            }
                            function finish() {
                                opened = ""
                                goOn()
                            }
                            Connections {
                                target: VaultController
                                enabled: unlockFirst.opened !== ""
                                function onStateChanged() {
                                    if (unlockFirst.ready(unlockFirst.opened))
                                        unlockFirst.finish()
                                }
                                function onMessageChanged() {
                                    if (unlockFirst.opened !== "" && VaultController.state === VaultController.Locked
                                            && VaultController.message !== "") {
                                        unlockFirst.problem = VaultController.message
                                        unlockFirst.opened = ""
                                    }
                                }
                            }
                        }
                        // Only the crown moves (the owner, D-074).
                        DialogWindow {
                            id: primaryWindow
                            fitsContent: true // only asks: as tall as its text (D-079)
                            objectName: "primaryWindow"
                            memoryName: "makePrimary"
                            transientParent: settingsWindow
                            property string vaultFile: ""
                            title: qsTr("Make primary vault")
                            standardButtons: Dialog.Cancel
                            rightButtons: Button {
                                objectName: "makePrimaryButton"
                                text: qsTr("Make primary")
                                onClicked: {
                                    VaultController.makePrimary(primaryWindow.vaultFile)
                                    primaryWindow.accept()
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Are you sure you want to make “%1” your primary vault? No passwords are moved: what is remembered in “%2” stays there.")
                                      .arg(VaultController.vaultNameOf(primaryWindow.vaultFile))
                                      .arg(VaultController.vaultNameOf(VaultController.primaryVault))
                                wrapMode: Text.Wrap
                            }
                        }
                        // Set PIN… and Change PIN…: the vault's own master password, typed
                        // now, and the new PIN (D-076).
                        AskWindow {
                            id: pinWindow
                            objectName: "pinWindow"
                            transientParent: settingsWindow
                            withNewPin: true
                            okText: qsTr("OK")
                            onRight: function(file) {
                                VaultController.setPinFor(file, newPinText)
                                accept()
                            }
                        }
                        // The fingerprint turned on: the standard dialog that asks the
                        // master password, typed, never a finger (the owner, D-043).
                        AskWindow {
                            id: fingerPasswordWindow
                            objectName: "fingerPasswordWindow"
                            transientParent: settingsWindow
                            typedOnly: true
                            onRight: function(file) {
                                VaultController.setQuickUnlock(true)
                                accept()
                            }
                        }
                        DialogWindow {
                            id: removePinWindow
                            fitsContent: true // only asks: as tall as its text (D-079)
                            objectName: "removePinWindow"
                            memoryName: "removePin"
                            transientParent: settingsWindow
                            property string vaultFile: ""
                            title: qsTr("Remove PIN")
                            standardButtons: Dialog.Cancel
                            rightButtons: Button {
                                objectName: "removePinButton"
                                text: qsTr("Remove")
                                onClicked: {
                                    VaultController.removePinFor(removePinWindow.vaultFile)
                                    removePinWindow.accept()
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Are you sure you want to remove the PIN of “%1”? After a lock, its password is asked instead.")
                                      .arg(VaultController.vaultNameOf(removePinWindow.vaultFile))
                                wrapMode: Text.Wrap
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
                                } else if (storage === "googledrive") {
                                    moveToDriveWindow.mode = "move"
                                    moveToDriveWindow.moveVault = moveChoice.vaultFile
                                    moveToDriveWindow.open()
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
                            fitsContent: true // only asks: as tall as its text (D-079)
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
                                Layout.fillWidth: true
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
                        GoogleDriveWindow {
                            id: moveToDriveWindow
                            objectName: "moveToGoogleDriveWindow"
                            transientParent: settingsWindow
                        }
                        // Delete…: first why it cannot be deleted now, in the owner's
                        // order; a locked vault asks its own password (D-083).
                        DialogWindow {
                            id: noDeleteWindow
                            fitsContent: true // only says: as tall as its text (D-079)
                            objectName: "noDeleteWindow"
                            transientParent: settingsWindow
                            property string vaultFile: ""
                            property int refusal: 0
                            title: qsTr("Delete the vault")
                            standardButtons: Dialog.Close
                            Label {
                                objectName: "dialogText"
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                readonly property string name: VaultController.vaultNameOf(noDeleteWindow.vaultFile)
                                text: noDeleteWindow.refusal === 1
                                      ? qsTr("“%1” is your primary vault and cannot be deleted while you have other vaults. To delete it, first make another vault primary with Make primary.").arg(name)
                                      : (noDeleteWindow.refusal === 2 ? qsTr("The vault “%1” is currently open.").arg(name)
                                                                      : qsTr("The vault “%1” is currently closed.").arg(name))
                                        + " " + qsTr("The vault must be locked before it can be deleted.")
                                wrapMode: Text.Wrap
                            }
                        }
                        DeleteVaultWindow {
                            id: deleteVaultWindow
                            transientParent: settingsWindow
                        }
                        DialogWindow {
                            id: forgetWindow
                            fitsContent: true // only asks: as tall as its text (D-079)
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
                                // Its paragraphs half a line apart (D-082).
                                textFormat: Text.RichText
                                text: forgetWindow.paragraphs([
                                    qsTr("Chrono Vault remembers every vault you have opened. If you no longer use a vault, you can tell Chrono Vault to forget it.") + " "
                                      + (forgetWindow.onWebDav
                                         ? qsTr("The vault in the remote location is not deleted, but the local copy on this computer is, together with any changes that are not synced yet.")
                                         : qsTr("Nothing is deleted, only the name of the vault is removed from the list of vaults.")),
                                    qsTr("Are you sure you want Chrono Vault to forget “%1”?").arg(VaultController.vaultNameOf(forgetWindow.vaultFile))])
                                wrapMode: Text.Wrap
                            }
                        }
                        DialogWindow {
                            id: disconnectWindow
                            fitsContent: true // only asks: as tall as its text (D-079)
                            objectName: "disconnectWindow"
                            memoryName: "disconnectWebDav2"
                            transientParent: settingsWindow
                            property string vaultFile: ""
                            property string storageName: ""
                            title: qsTr("Disconnect from %1").arg(storageName)
                            standardButtons: Dialog.Cancel
                            rightButtons: Button {
                                objectName: "disconnectButton"
                                text: qsTr("Disconnect")
                                onClicked: {
                                    VaultController.disconnectVault(disconnectWindow.vaultFile)
                                    disconnectWindow.accept()
                                }
                            }
                            Label {
                                objectName: "dialogText"
                                Layout.fillWidth: true
                                // Three paragraphs; Disconnect deletes nothing, on any storage (D-060).
                                textFormat: Text.RichText
                                text: disconnectWindow.paragraphs([
                                    qsTr("Chrono Vault keeps a local copy of every vault that is stored in a remote location, like %1, so you can keep working while you are offline. Changes are synced automatically once your connection is restored.").arg(disconnectWindow.storageName),
                                    qsTr("Disconnecting from the remote vault makes the local copy a separate individual vault. Both vaults continue to exist and nothing is deleted, but changes in one vault are no longer synced with the other vault."),
                                    qsTr("This action cannot be undone. Are you sure you want to disconnect from “%1”?").arg(VaultController.vaultNameOf(disconnectWindow.vaultFile))])
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
        DialogWindow {
            id: keyCatcher
            objectName: "keyCatcher"
            transientParent: settingsWindow
            title: qsTr("Press the new key")
            standardButtons: Dialog.Cancel
            // The keyboard in the catcher, never on Cancel: Space, Return and
            // Tab arrive as keys (the design of every dialog, D-082).
            defaultItem: catcher
            Component.onCompleted: standardButton(Dialog.Cancel).focusPolicy = Qt.NoFocus
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
        }

        // A key already in use, asked as Chrono Viewer asks it: OK gives the
        // key to the new action and takes that one key from the old one;
        // Cancel, the default, changes nothing. The design of every dialog (D-082).
        DialogWindow {
            id: keyTaken
            objectName: "keyTaken"
            transientParent: settingsWindow
            title: qsTr("Keyboard shortcuts")
            standardButtons: Dialog.Ok | Dialog.Cancel
            defaultButton: Dialog.Cancel
            Component.onCompleted: {
                standardButton(Dialog.Ok).objectName = "keyTakenOk"
                standardButton(Dialog.Cancel).objectName = "keyTakenCancel"
            }
            property string informative: ""
            function ask(key, oldName, newName) {
                informative = qsTr("%1 is assigned to “%2”. Reassign it to “%3”?").arg(key).arg(oldName).arg(newName)
                open()
            }
            onAccepted: {
                settingsWindow.giveKey(keyCatcher.actionId, keyCatcher.pendingKey)
                keyCatcher.pendingKey = ""
            }
            onRejected: keyCatcher.pendingKey = ""
            Label {
                text: qsTr("This key has already been assigned")
                // In bold, as first line, no "?" picture (the owner, Q22, D-082).
                font.bold: true
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                text: keyTaken.informative
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        // OK, Cancel and Restore defaults at the bottom right.
        ExactRow {
            id: bottomRow
            Layout.fillWidth: true
            gap: 8 * Appearance.factor
            // Buttons alike only inside a group (the owner, D-082): Create
            // vault and Open vault, Restore defaults on its own, OK and
            // Cancel; 28 high, at least 88 wide, as in every dialog.
            readonly property real leftWidth: Math.ceil(Math.max(88 * Appearance.factor,
                createVaultButton.implicitWidth, openVaultButton.implicitWidth))
            readonly property real restoreWidth: Math.ceil(Math.max(88 * Appearance.factor, restoreDefaultsButton.implicitWidth))
            readonly property real rightWidth: Math.ceil(Math.max(88 * Appearance.factor,
                okButton.implicitWidth, cancelButton.implicitWidth))
            readonly property real buttonHigh: Math.round(28 * Appearance.factor)
            // The Vaults page's own buttons, at the section's left edge (D-062).
            Item {
                visible: settingsWindow.onVaults
                Layout.preferredWidth: Math.max(0, pageScroll.x - bottomRow.gap)
            }
            Button {
                id: createVaultButton
                objectName: "createVaultButton"
                Layout.preferredWidth: bottomRow.leftWidth
                Layout.preferredHeight: bottomRow.buttonHigh
                visible: settingsWindow.onVaults
                text: qsTr("Create vault")
                onClicked: settingsWindow.createVaultRequested()
            }
            Button {
                id: openVaultButton
                objectName: "openVaultButton"
                Layout.preferredWidth: bottomRow.leftWidth
                Layout.preferredHeight: bottomRow.buttonHigh
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
                id: restoreDefaultsButton
                objectName: "restoreDefaultsButton"
                Layout.preferredWidth: bottomRow.restoreWidth
                Layout.preferredHeight: bottomRow.buttonHigh
                // Nothing to put back on the Vaults page (D-062).
                visible: settingsWindow.restorable
                text: qsTr("Restore defaults")
                onClicked: settingsWindow.restoreDefaults()
            }
            // Restore defaults its own group, apart from OK and Cancel (D-082).
            Item {
                visible: restoreDefaultsButton.visible
                Layout.preferredWidth: 16 * Appearance.factor
            }
            Button {
                id: okButton
                objectName: "okButton"
                Layout.preferredWidth: bottomRow.rightWidth
                Layout.preferredHeight: bottomRow.buttonHigh
                text: qsTr("OK")
                highlighted: true
                icon.source: "qrc:/icons/ok.svg"
                icon.width: Math.round(16 * Appearance.factor)
                icon.height: Math.round(16 * Appearance.factor)
                icon.color: settingsWindow.palette.buttonText
                onClicked: settingsWindow.accept()
            }
            Button {
                id: cancelButton
                objectName: "cancelButton"
                Layout.preferredWidth: bottomRow.rightWidth
                Layout.preferredHeight: bottomRow.buttonHigh
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
