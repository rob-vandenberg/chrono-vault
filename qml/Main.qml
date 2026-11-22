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
// Main.qml 0.0.25

// --- Version history ----------------------------------------------------
// v0.0.25: Autolock when the main window is closed; Settings > Security opens Change the Master Password (D-040)
// v0.0.24: The menus after the reference program's, with their shortcuts (D-039)
// v0.0.22: File > Export… (D-037)
// v0.0.19: File > Import… (D-035)
// v0.0.17: Item > Move to Trash in place of Delete item (D-033)
// v0.0.16: New Item shows a ring when it has the keyboard focus (P12)
// v0.0.15: New vault and Open vault also with a vault unlocked; Change master password or keyfile (D-030, D-031)
// v0.0.14: New Item lists every category with its templates (D-027)
// v0.0.10: The program's colours from the start (D-023); New Item a flat fill in the program's blue
// v0.0.9: The update's windows as in Chrono Mail in place of the line at the bottom (D-024); one bar with the
//         menus and the buttons (D-025)
// v0.0.8: The About window can be found by its name (tests)
// v0.0.7: The program's font, so the UI scaling reaches every control (D-023)
// v0.0.5: F1 opens About; the Update window; a Quit for an update waits for the editor or a save and goes on
// v0.0.4: The version in the title; hiding to the tray, Quit and Ctrl+Q
// v0.0.3: The start, unlock and vault pages, the tool bar, the message line with Try saving again
// v0.0.1: Initial release

ApplicationWindow {
    id: mainWindow
    // The program's font, so the UI scaling reaches every control (D-023).
    font: Appearance.font
    // The program's colours, never the desktop theme's (D-023).
    Component.onCompleted: Appearance.dress(this)

    width: Math.round(960 * Appearance.factor)
    height: Math.round(640 * Appearance.factor)
    // Shown by the program as it starts: normally, minimized, or not at all
    // when "Start Chrono Vault minimized" keeps it in the tray (D-019).
    visible: false
    title: VaultController.state === VaultController.Unlocked
           ? qsTr("%1 – Chrono Vault %2").arg(VaultController.vaultName).arg(VaultController.version)
           : qsTr("Chrono Vault %1").arg(VaultController.version)

    readonly property bool busy: VaultController.state === VaultController.Busy

    readonly property bool unlocked: VaultController.state === VaultController.Unlocked

    // A text field has the keyboard: the menus' shortcuts stand aside, so
    // Backspace deletes a letter, never the item (D-039).
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.cursorPosition !== undefined
    function keys(sequence) {
        return typing ? "" : sequence
    }

    readonly property string supportWebsite: "https://github.com/rob-vandenberg/chrono-vault"

    // The item the Item menu acts on: the one shown, while the editor is not
    // open (D-039).
    readonly property bool itemFree: unlocked && vaultPage.chosenItemId !== "" && !vaultPage.editing
    readonly property var chosenItem: itemFree && VaultController.revision >= 0
                                      ? VaultController.item(vaultPage.chosenItemId) : ({})
    readonly property bool hasSensitive: (chosenItem.fields || []).some(function(field) {
        return !field.deleted && field.kind !== "hidden" && field.sensitive && field.value !== ""
    })
    // The value of the chosen item's first field of a kind: "username",
    // "email", "password", "url" or "totp"; empty when it has none.
    function fieldValue(kind) {
        const fields = (chosenItem.fields || []).filter(function(field) {
            return !field.deleted && field.kind !== "hidden" && field.kind !== "section" && field.value !== ""
        })
        const matches = {
            username: function(field) { return field.kind === "username" || field.role === "username" },
            email: function(field) { return field.kind === "email" },
            password: function(field) { return field.kind === "password" || field.role === "password" },
            url: function(field) { return field.kind === "url" },
            totp: function(field) { return field.kind === "totp" }
        }
        let found = fields.find(matches[kind])
        // A password of an imported item is a sensitive text field.
        if (!found && kind === "password")
            found = fields.find(function(field) { return field.kind === "text" && field.sensitive })
        return found ? found.value : ""
    }
    // An address without its scheme opens as https; "nas.local:5000" is a
    // host and a port, not a scheme.
    function addressToOpen(given) {
        const address = given.trim()
        return /^[a-z][a-z0-9+.-]*:\/\//i.test(address) || /^(mailto|tel):/i.test(address)
               ? address : "https://" + address
    }
    function launch(address) {
        Qt.openUrlExternally(addressToOpen(address))
    }
    function changeMasterPassword() {
        vaultPage.whenEditorFree(function() { masterPasswordWindow.ask() })
    }

    // The categories the left column shows, each with its templates; one
    // without a template is left out (D-027).
    readonly property var newItemGroups: {
        if (VaultController.revision < 0)
            return []
        const templates = VaultController.templates()
        return VaultController.categories()
            .filter(function(category) { return !category.hidden })
            .map(function(category) {
                return { id: category.id, name: category.name,
                         templates: templates.filter(function(one) { return one.categoryId === category.id }) }
            })
            .filter(function(group) { return group.templates.length > 0 })
    }

    // A menu of the bar: the style's own entries, with the shortcut at the
    // right, and as wide as its widest entry (D-039).
    component MainMenu: Menu {
        delegate: MenuItem {}
        width: {
            let widest = implicitWidth
            for (let index = 0; index < count; ++index) {
                const entry = itemAt(index)
                if (entry)
                    widest = Math.max(widest, entry.implicitWidth + leftPadding + rightPadding)
            }
            return widest
        }
    }

    // One bar at the top (D-025): the menus on the left, the buttons on the
    // right. What needs an unlocked vault is greyed until then.
    header: ToolBar {
        objectName: "topBar"
        RowLayout {
            anchors.fill: parent
            anchors.rightMargin: Math.round(8 * Appearance.factor)
            spacing: Math.round(6 * Appearance.factor)

            // The menus after the reference program's, with only what Chrono
            // Vault does (D-039). A shortcut never acts while a text field is
            // being typed in.
            MenuBar {
                objectName: "menuBar"
                background: Item {}
                MainMenu {
                    objectName: "fileMenu"
                    title: qsTr("&File")
                    Action {
                        text: qsTr("New vault")
                        enabled: !mainWindow.busy
                        onTriggered: mainWindow.newVault()
                    }
                    Action {
                        text: qsTr("Open vault")
                        enabled: !mainWindow.busy
                        onTriggered: mainWindow.openVault()
                    }
                    Action {
                        text: qsTr("Change Master Password")
                        enabled: mainWindow.unlocked
                        onTriggered: mainWindow.changeMasterPassword()
                    }
                    MenuSeparator {}
                    // Not built yet.
                    Action {
                        text: qsTr("Backup")
                        enabled: false
                    }
                    Action {
                        text: qsTr("Restore")
                        enabled: false
                    }
                    MenuSeparator {}
                    Action {
                        text: qsTr("Import")
                        enabled: mainWindow.unlocked
                        onTriggered: vaultPage.whenEditorFree(function() { importWindow.start() })
                    }
                    Action {
                        text: qsTr("Export")
                        enabled: mainWindow.unlocked
                        onTriggered: vaultPage.whenEditorFree(function() { exportWindow.ask() })
                    }
                    MenuSeparator {}
                    Action {
                        text: qsTr("Preferences")
                        onTriggered: settingsWindow.showWindow()
                    }
                    MenuSeparator {}
                    Action {
                        objectName: "quitAction"
                        text: qsTr("Quit")
                        shortcut: "Ctrl+Q"
                        onTriggered: mainWindow.requestQuit()
                    }
                }
                MainMenu {
                    objectName: "viewMenu"
                    title: qsTr("&View")
                    Action {
                        objectName: "showItemCountsAction"
                        text: qsTr("Show Item Counts")
                        checkable: true
                        checked: VaultController.showItemCounts
                        onToggled: {
                            VaultController.showItemCounts = checked
                            checked = Qt.binding(function() { return VaultController.showItemCounts })
                        }
                    }
                    Action {
                        objectName: "hideSidebarAction"
                        text: qsTr("Hide Sidebar")
                        checkable: true
                        checked: VaultController.sidebarHidden
                        onToggled: {
                            VaultController.sidebarHidden = checked
                            checked = Qt.binding(function() { return VaultController.sidebarHidden })
                        }
                    }
                    Action {
                        objectName: "hideSensitiveAction"
                        text: qsTr("Hide Sensitive")
                        checkable: true
                        checked: VaultController.hideSensitive
                        onToggled: {
                            VaultController.hideSensitive = checked
                            checked = Qt.binding(function() { return VaultController.hideSensitive })
                        }
                    }
                    MenuSeparator {}
                    Action {
                        objectName: "showCategoriesAction"
                        text: qsTr("Categories")
                        checkable: true
                        checked: VaultController.showCategories
                        onToggled: {
                            VaultController.showCategories = checked
                            checked = Qt.binding(function() { return VaultController.showCategories })
                        }
                    }
                    Action {
                        objectName: "showTagsAction"
                        text: qsTr("Tags")
                        checkable: true
                        checked: VaultController.showTags
                        onToggled: {
                            VaultController.showTags = checked
                            checked = Qt.binding(function() { return VaultController.showTags })
                        }
                    }
                    Action {
                        objectName: "showOthersAction"
                        text: qsTr("Others")
                        checkable: true
                        checked: VaultController.showOthers
                        onToggled: {
                            VaultController.showOthers = checked
                            checked = Qt.binding(function() { return VaultController.showOthers })
                        }
                    }
                }
                MainMenu {
                    objectName: "itemMenu"
                    title: qsTr("&Item")
                    Action {
                        objectName: "archiveAction"
                        text: qsTr("Archive")
                        property string shownKeys: "Shift+Backspace"
                        shortcut: mainWindow.keys(shownKeys)
                        enabled: mainWindow.itemFree && mainWindow.chosenItem.archived !== true
                                 && mainWindow.chosenItem.trashed !== true
                        onTriggered: vaultPage.archiveChosen()
                    }
                    Action {
                        objectName: "moveToTrashAction"
                        text: qsTr("Move to Trash")
                        property string shownKeys: "Backspace"
                        shortcut: mainWindow.keys(shownKeys)
                        // Not for an item already in the Trash.
                        enabled: mainWindow.itemFree && mainWindow.chosenItem.trashed !== true
                        onTriggered: vaultPage.deleteChosen()
                    }
                    MenuSeparator {}
                    Action {
                        objectName: "copyUsernameAction"
                        text: qsTr("Copy Username")
                        property string shownKeys: "Shift+Ctrl+U"
                        shortcut: mainWindow.keys(shownKeys)
                        enabled: mainWindow.itemFree && mainWindow.fieldValue("username") !== ""
                        onTriggered: VaultController.copyToClipboard(mainWindow.fieldValue("username"))
                    }
                    Action {
                        objectName: "copyEmailAction"
                        text: qsTr("Copy E-mail")
                        property string shownKeys: "Shift+Ctrl+E"
                        shortcut: mainWindow.keys(shownKeys)
                        enabled: mainWindow.itemFree && mainWindow.fieldValue("email") !== ""
                        onTriggered: VaultController.copyToClipboard(mainWindow.fieldValue("email"))
                    }
                    Action {
                        objectName: "copyPasswordAction"
                        text: qsTr("Copy Password")
                        property string shownKeys: "Shift+Ctrl+P"
                        shortcut: mainWindow.keys(shownKeys)
                        enabled: mainWindow.itemFree && mainWindow.fieldValue("password") !== ""
                        onTriggered: VaultController.copyToClipboard(mainWindow.fieldValue("password"))
                    }
                    Action {
                        objectName: "showPasswordsAction"
                        text: qsTr("Show Passwords")
                        property string shownKeys: "Shift+Ctrl+H"
                        shortcut: mainWindow.keys(shownKeys)
                        // Shows them, and hides them again when chosen once more.
                        enabled: mainWindow.itemFree && VaultController.hideSensitive && mainWindow.hasSensitive
                        onTriggered: vaultPage.revealAll = !vaultPage.revealAll
                    }
                    Action {
                        objectName: "copyUrlAction"
                        text: qsTr("Copy URL")
                        property string shownKeys: "Shift+Ctrl+R"
                        shortcut: mainWindow.keys(shownKeys)
                        enabled: mainWindow.itemFree && mainWindow.fieldValue("url") !== ""
                        onTriggered: VaultController.copyToClipboard(mainWindow.fieldValue("url"))
                    }
                    Action {
                        objectName: "copyOtpAction"
                        text: qsTr("Copy OTP")
                        property string shownKeys: "Shift+Ctrl+T"
                        shortcut: mainWindow.keys(shownKeys)
                        // Greyed too when the secret makes no code (a damaged import).
                        enabled: mainWindow.itemFree && mainWindow.fieldValue("totp") !== ""
                                 && VaultController.oneTimeCode(mainWindow.fieldValue("totp")) !== ""
                        onTriggered: VaultController.copyToClipboard(VaultController.oneTimeCode(mainWindow.fieldValue("totp")))
                    }
                    Action {
                        objectName: "launchUrlAction"
                        text: qsTr("Launch URL")
                        property string shownKeys: "Shift+Ctrl+L"
                        shortcut: mainWindow.keys(shownKeys)
                        enabled: mainWindow.itemFree && mainWindow.fieldValue("url") !== ""
                        onTriggered: mainWindow.launch(mainWindow.fieldValue("url"))
                    }
                }
                MainMenu {
                    objectName: "toolsMenu"
                    title: qsTr("&Tools")
                    Action {
                        objectName: "lockAction"
                        text: qsTr("Lock vault")
                        property string shownKeys: "Ctrl+L"
                        shortcut: mainWindow.keys(shownKeys)
                        enabled: mainWindow.unlocked
                        onTriggered: VaultController.lock()
                    }
                    Action {
                        objectName: "clearClipboardAction"
                        text: qsTr("Clear Clipboard")
                        property string shownKeys: "Ctrl+B"
                        shortcut: mainWindow.keys(shownKeys)
                        onTriggered: VaultController.clearClipboard()
                    }
                    Action {
                        objectName: "generatePasswordAction"
                        text: qsTr("Generate Password")
                        property string shownKeys: "Ctrl+G"
                        shortcut: mainWindow.keys(shownKeys)
                        onTriggered: toolsGenerator.open()
                    }
                }
                MainMenu {
                    objectName: "helpMenu"
                    title: qsTr("&Help")
                    Action {
                        text: qsTr("Check for updates")
                        onTriggered: {
                            aboutWindow.open()
                            aboutWindow.checkNow()
                        }
                    }
                    Action {
                        objectName: "supportWebsiteAction"
                        text: qsTr("Support website")
                        onTriggered: Qt.openUrlExternally(mainWindow.supportWebsite)
                    }
                    Action {
                        text: qsTr("About")
                        shortcut: "F1"
                        onTriggered: aboutWindow.open()
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                objectName: "newItemButton"
                text: qsTr("New Item")
                icon.source: "qrc:/icons/plus.svg"
                icon.color: mainWindow.palette.highlightedText
                // A flat fill in the program's blue, as the main action: darker
                // under the mouse and when pressed, greyed without a vault.
                palette.buttonText: mainWindow.palette.highlightedText
                background: Rectangle {
                    objectName: "newItemFill"
                    implicitWidth: Math.round(80 * Appearance.factor)
                    implicitHeight: Math.round(28 * Appearance.factor)
                    radius: Math.round(3 * Appearance.factor)
                    color: !parent.enabled ? mainWindow.palette.mid
                           : parent.down ? Qt.darker(mainWindow.palette.highlight, 1.3)
                           : parent.hovered ? Qt.darker(mainWindow.palette.highlight, 1.15)
                           : mainWindow.palette.highlight
                    // The keyboard focus, seen as a ring in the text's colour (P12).
                    border.width: parent.visualFocus ? Math.max(2, Math.round(2 * Appearance.factor)) : 0
                    border.color: mainWindow.palette.highlightedText
                }
                enabled: mainWindow.unlocked
                onClicked: newItemMenu.popup(this, 0, height)
                // Every category shown in the left column, with its templates
                // (D-027).
                Menu {
                    id: newItemMenu
                    objectName: "newItemMenu"
                    Instantiator {
                        model: mainWindow.newItemGroups
                        delegate: Menu {
                            id: categoryMenu
                            required property var modelData
                            title: modelData.name
                            Instantiator {
                                model: categoryMenu.modelData.templates
                                delegate: MenuItem {
                                    required property var modelData
                                    text: modelData.name
                                    onTriggered: vaultPage.newItem(modelData.id)
                                }
                                onObjectAdded: function(index, object) { categoryMenu.insertItem(index, object) }
                                onObjectRemoved: function(index, object) { categoryMenu.removeItem(object) }
                            }
                        }
                        onObjectAdded: function(index, object) { newItemMenu.insertMenu(index, object) }
                        onObjectRemoved: function(index, object) { newItemMenu.removeMenu(object) }
                    }
                }
            }
            ToolSeparator {}
            Button {
                objectName: "generateButton"
                text: qsTr("Generate Password")
                icon.source: "qrc:/icons/generate.svg"
                icon.color: mainWindow.palette.buttonText
                onClicked: toolsGenerator.open()
            }
            ToolButton {
                objectName: "lockButton"
                icon.source: "qrc:/icons/lock.svg"
                icon.color: mainWindow.palette.buttonText
                enabled: mainWindow.unlocked
                onClicked: VaultController.lock()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Lock the vault")
            }
            ToolButton {
                objectName: "preferencesButton"
                icon.source: "qrc:/icons/settings.svg"
                icon.color: mainWindow.palette.buttonText
                onClicked: settingsWindow.showWindow()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Preferences")
            }
        }
    }

    // A new or another vault: with a vault unlocked, an edit not saved yet is
    // asked about first (D-030).
    function newVault() {
        if (unlocked)
            vaultPage.whenEditorFree(function() { welcomePage.newVault() })
        else
            welcomePage.newVault()
    }
    function openVault() {
        if (unlocked)
            vaultPage.whenEditorFree(function() { welcomePage.openVault() })
        else
            welcomePage.openVault()
    }

    ImportWindow {
        id: importWindow
    }

    ExportWindow {
        id: exportWindow
    }

    MasterPasswordWindow {
        id: masterPasswordWindow
        objectName: "masterPasswordWindow"
    }

    // The generator on its own (Tools, the bar): its password is copied.
    GeneratorDialog {
        id: toolsGenerator
        Component.onCompleted: standardButton(Dialog.Ok).text = qsTr("Copy")
        onUsePassword: function(password) { VaultController.copyToClipboard(password) }
    }

    StackLayout {
        anchors.fill: parent
        enabled: !mainWindow.busy
        currentIndex: VaultController.state === VaultController.Unlocked ? 2
                      : (VaultController.vaultPath === "" ? 0 : 1)

        WelcomePage { id: welcomePage }
        UnlockPage {}
        VaultPage {
            id: vaultPage
            onNewVaultRequested: mainWindow.newVault()
            onOpenVaultRequested: mainWindow.openVault()
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: mainWindow.busy
        visible: running
    }

    // Quit, from the tray or with Ctrl+Q: asks about unsaved work like every
    // close (D-016), then ends the program.
    property bool quitting: false
    function requestQuit() {
        quitting = true
        if (!visible)
            show()
        close()
    }

    // Ctrl+Q in the program's other windows; in this one File > Quit has it.
    Shortcut {
        sequences: ["Ctrl+Q"]
        context: Qt.ApplicationShortcut
        enabled: !mainWindow.active
        onActivated: mainWindow.requestQuit()
    }

    // Closing never drops work without asking (OR-15).
    property bool quitAnyway: false
    property bool closeWhenSaved: false
    property bool closeWhenSavedQuits: false
    // The quit dialog stands in the way of a Quit (not of a close to the tray).
    property bool quitDialogFromQuit: false
    onClosing: function(close) {
        // Logging out or shutting down: never hold it up. What is being typed is
        // stored, and a running save finishes before the program ends (OR-15).
        if (VaultController.isSessionEnding()) {
            if (vaultPage.hasUnsavedEdit())
                vaultPage.storeEdit()
            settingsWindow.close()
            Qt.quit()
            return
        }
        // Closing the window keeps Chrono Vault running in the tray (D-019).
        if (!quitting && !quitAnyway && VaultController.keepInTray && VaultController.trayAvailable) {
            close.accepted = false
            settingsWindow.close()
            hide()
            // Autolock when the main window is closed (D-040); a finger still
            // asked for is no longer asked: the reader is given back (D-042).
            VaultController.stopQuickUnlock()
            VaultController.mainWindowClosed()
            return
        }
        if (!quitAnyway) {
            // A close that is held up leaves no "quitting" mark behind: a Quit
            // that is cancelled must not turn the next close into a Quit. What
            // follows remembers whether it was one.
            const wasQuit = quitting
            quitting = false
            // A Quit that waits for the editor or a running save goes on by
            // itself afterwards, so an update stays armed; only a Quit the
            // person abandons (Cancel in the editor's box, Retry or Cancel in
            // the quit dialog) disarms it.
            if (vaultPage.hasUnsavedEdit()) {
                close.accepted = false
                vaultPage.whenEditorFree(function() { wasQuit ? mainWindow.requestQuit() : mainWindow.close() },
                                         function() { if (wasQuit) Updater.quitHeldUp() })
                return
            }
            if (VaultController.saveFailed || VaultController.hasKeptChanges) {
                close.accepted = false
                quitDialogFromQuit = wasQuit
                quitDialog.open()
                return
            }
            if (VaultController.unsavedChanges) {
                // A save is still running: close once it has finished.
                close.accepted = false
                closeWhenSaved = true
                closeWhenSavedQuits = wasQuit
                return
            }
        }
        // The close is accepted: the program ends (it never ends by itself when
        // its last window goes, D-019).
        settingsWindow.close()
        Qt.quit()
    }

    Connections {
        target: VaultController
        function onUnsavedChangesChanged() {
            if (mainWindow.closeWhenSaved && !VaultController.unsavedChanges) {
                mainWindow.closeWhenSaved = false
                mainWindow.closeWhenSavedQuits ? mainWindow.requestQuit() : mainWindow.close()
            }
        }
        function onSaveFailedChanged() {
            if (mainWindow.closeWhenSaved && VaultController.saveFailed) {
                mainWindow.closeWhenSaved = false
                mainWindow.quitDialogFromQuit = mainWindow.closeWhenSavedQuits
                quitDialog.open()
            }
        }
    }

    DialogWindow {

        id: quitDialog

        memoryName: "quit"
        objectName: "quitDialog"
        title: qsTr("A change is not saved")
        standardButtons: VaultController.state === VaultController.Unlocked
                         ? Dialog.Retry | Dialog.Discard | Dialog.Cancel
                         : Dialog.Discard | Dialog.Cancel
        ColumnLayout {
            Label {
                text: VaultController.state === VaultController.Unlocked
                      ? qsTr("Your last change could not be saved. If you quit now, it is lost. The vault on disk is unchanged.")
                      : qsTr("This vault holds a change that could not be saved yet; unlocking it saves it. If you quit now, it is lost.")
                wrapMode: Text.Wrap
                Layout.preferredWidth: Math.round(380 * Appearance.factor)
            }
        }
        // Retry or Cancel abandon the Quit: an update armed by it is not set
        // off at a later, ordinary quit. Discard goes on with the Quit.
        function quitAbandoned() {
            if (mainWindow.quitDialogFromQuit)
                Updater.quitHeldUp()
            mainWindow.quitDialogFromQuit = false
        }
        onAccepted: {
            quitAbandoned()
            VaultController.retrySave()
        }
        onRejected: quitAbandoned()
        onDiscarded: {
            quitDialog.close()
            mainWindow.quitAnyway = true
            mainWindow.close()
        }
    }

    footer: ColumnLayout {
        spacing: 0
        visible: VaultController.message !== ""
                 || (VaultController.saveFailed && VaultController.state === VaultController.Unlocked)
        Label {
            visible: VaultController.message !== ""
            text: VaultController.message
            font.bold: VaultController.messageIsTrouble
            wrapMode: Text.Wrap
            padding: Math.round(8 * Appearance.factor)
            Layout.fillWidth: true
        }
        RowLayout {
            visible: VaultController.saveFailed && VaultController.state === VaultController.Unlocked
            Layout.fillWidth: true
            Label {
                text: qsTr("A change is not saved yet. The vault on disk is unchanged.")
                font.bold: true
                wrapMode: Text.Wrap
                padding: Math.round(8 * Appearance.factor)
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Try saving again")
                onClicked: VaultController.retrySave()
                Layout.rightMargin: Math.round(8 * Appearance.factor)
            }
        }
    }

    SettingsWindow {
        id: settingsWindow
        onChangeMasterPasswordRequested: mainWindow.changeMasterPassword()
    }

    // About, with F1 (D-021), and the update's windows as in Chrono Mail (D-024).
    AboutWindow {
        id: aboutWindow
        objectName: "aboutWindow"
        onCheckRequested: Updater.checkNow()
    }
    UpdateAvailableWindow { id: updateAvailableWindow }
    UpdateDownloadedWindow { id: updateDownloadedWindow }
    UpdatedWindow { id: updatedWindow }

    // A found update: one asked for in About comes forward at once; one found by
    // itself is shown without taking the keyboard, and only while the main
    // window is on screen and the settings are not open (it waits till then).
    property bool updateWaiting: false
    function offerWaitingUpdate() {
        if (updateWaiting && visible && !settingsWindow.visible && Updater.state === Updater.Available) {
            updateWaiting = false
            updateAvailableWindow.openQuietly()
        }
    }
    onVisibleChanged: offerWaitingUpdate()
    Connections {
        target: settingsWindow
        function onVisibleChanged() { mainWindow.offerWaitingUpdate() }
    }
    Connections {
        target: Updater
        function onQuitRequested() { mainWindow.requestQuit() }
        function onOffered(byHand) {
            if (byHand) {
                aboutWindow.accept()
                mainWindow.updateWaiting = false
                updateAvailableWindow.open()
            } else if (!updateAvailableWindow.visible) {
                mainWindow.updateWaiting = true
                mainWindow.offerWaitingUpdate()
            }
        }
        // Downloaded: the first window closes as the next one opens.
        function onStateChanged() {
            if (Updater.state === Updater.Downloaded && updateAvailableWindow.visible) {
                updateAvailableWindow.accept()
                updateDownloadedWindow.open()
            } else if (Updater.waitsForRestartChoice && !updatedWindow.visible) {
                updateDownloadedWindow.accept()
                updatedWindow.open()
            }
        }
    }

    WindowMemory {
        window: mainWindow
        name: "main"
    }
}
