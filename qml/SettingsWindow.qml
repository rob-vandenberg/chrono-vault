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
// SettingsWindow.qml 0.0.6

// --- Version history ----------------------------------------------------
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

    // What the window shows, until OK keeps it.
    property bool keepInTray: false
    property bool startAtStartup: false
    property bool startMinimized: false
    property int checkFrequency: 0
    property int autoLockMinutes: 0
    property int clipboardClearSeconds: 0

    function loadSettings() {
        keepInTray = VaultController.keepInTray
        startAtStartup = VaultController.startAtStartup
        startMinimized = VaultController.startMinimized
        checkFrequency = Updater.frequency
        autoLockMinutes = VaultController.autoLockMinutes
        clipboardClearSeconds = VaultController.clipboardClearSeconds
    }

    // OK: every topic's changes are kept, whichever topic is shown.
    function accept() {
        // A number still being typed counts.
        okButton.forceActiveFocus()
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
        if (VaultController.clipboardClearSeconds !== clipboardClearSeconds)
            VaultController.clipboardClearSeconds = clipboardClearSeconds
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
    onVisibleChanged: if (!visible) problem = ""

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
            break
        case 1:
            autoLockMinutes = defaults.autoLockMinutes
            break
        case 2:
            clipboardClearSeconds = defaults.clipboardClearSeconds
            break
        }
    }

    title: qsTr("Settings – Chrono Vault %1").arg(VaultController.version)
    width: 640
    height: 400
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

    // The topics' list is as wide as its longest name and no wider.
    FontMetrics {
        id: topicMetrics
        font: settingsWindow.font
    }
    function widestTopic() {
        let widest = 0
        for (let i = 0; i < topics.model.length; ++i)
            widest = Math.max(widest, topicMetrics.advanceWidth(topics.model[i]))
        return Math.ceil(widest)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // The topics: a framed panel, the chosen one marked across its
            // whole width.
            Rectangle {
                objectName: "topicsPanel"
                Layout.fillHeight: true
                Layout.preferredWidth: settingsWindow.widestTopic() + 2 * topicPadding + 2 * border.width
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
                    model: [qsTr("General"), qsTr("Security"), qsTr("Clipboard")]
                    delegate: Rectangle {
                        id: topic
                        required property int index
                        required property string modelData
                        readonly property bool chosen: ListView.isCurrentItem
                        width: ListView.view.width
                        height: topicText.implicitHeight + 6
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

            StackLayout {
                currentIndex: topics.currentIndex
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    spacing: 8
                    Label {
                        text: qsTr("General")
                        font.pointSize: 14
                    }
                    CheckBox {
                        objectName: "keepInTrayBox"
                        text: qsTr("Keep running in the system tray when the window is closed")
                        checked: settingsWindow.keepInTray
                        enabled: VaultController.trayAvailable
                        onToggled: settingsWindow.keepInTray = checked
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
                    spacing: 8
                    Label {
                        text: qsTr("Security")
                        font.pointSize: 14
                    }
                    Label {
                        text: qsTr("Lock the vault after this many minutes without use (0 means never):")
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    SpinBox {
                        objectName: "autoLockBox"
                        from: 0
                        to: 240
                        value: settingsWindow.autoLockMinutes
                        editable: true
                        onValueModified: settingsWindow.autoLockMinutes = value
                    }
                    Item { Layout.fillHeight: true }
                }

                ColumnLayout {
                    spacing: 8
                    Label {
                        text: qsTr("Clipboard")
                        font.pointSize: 14
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
            }
        }

        // OK, Cancel and Restore defaults at the bottom right.
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
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
                icon.width: 16
                icon.height: 16
                icon.color: settingsWindow.palette.buttonText
                onClicked: settingsWindow.accept()
            }
            Button {
                objectName: "cancelButton"
                text: qsTr("Cancel")
                icon.source: "qrc:/icons/cancel.svg"
                icon.width: 16
                icon.height: 16
                icon.color: settingsWindow.palette.buttonText
                onClicked: settingsWindow.close()
            }
        }
    }
}
