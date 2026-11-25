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
import QtQuick.Layouts

// --- Version ------------------------------------------------------------
// EmptyList.qml 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Exact sizes (D-069)
// v0.6.86: The lists of the audit (D-065)
// v0.5.83: Initial release (D-063)

// What a list without items shows: an open, empty vault and why; a search
// that finds nothing only says so.
ExactColumn {
    id: emptyList
    property var categories: []
    signal settingsRequested()
    gap: 4 * Appearance.factor

    readonly property string filter: VaultController.filter
    readonly property bool searching: VaultController.searchText !== ""
    readonly property bool forNewItems: filter === "" || filter.startsWith("category:")
    // A list of the audit that needs the online checks, and how they stand
    // (D-065); empty for any other.
    readonly property string onlineState: filter === "audit:leaked" ? VaultController.audit.leakState
                                          : filter === "audit:breached" ? VaultController.audit.breachState : ""
    readonly property string headline: {
        if (searching)
            return qsTr("Nothing found for “%1”").arg(VaultController.searchText)
        if (filter === "")
            return qsTr("No items yet")
        if (filter === "favourites")
            return qsTr("No favorites yet")
        if (filter === "archive")
            return qsTr("The Archive is empty")
        if (filter === "trash")
            return qsTr("The Trash is empty")
        if (onlineState === "off" && !VaultController.checkLeaks)
            return qsTr("The check for leaked passwords and breached websites is off")
        if (onlineState === "failed")
            return qsTr("Not checked: Have I Been Pwned could not be reached")
        if (onlineState !== "" && onlineState !== "done" || filter.startsWith("audit:") && !VaultController.audit.ready)
            return qsTr("Checking…")
        if (filter === "audit:leaked")
            return qsTr("No leaked passwords")
        if (filter === "audit:reused")
            return qsTr("No reused passwords")
        if (filter === "audit:weak")
            return qsTr("No weak passwords")
        if (filter === "audit:old")
            return qsTr("No old passwords")
        if (filter === "audit:breached")
            return qsTr("No breached websites")
        if (filter.startsWith("tag:"))
            return qsTr("No items with the tag “%1”").arg(filter.slice(4))
        const id = filter.slice("category:".length)
        const found = categories.find(function(category) { return category.id === id })
        return qsTr("No items in %1").arg(found ? found.name : "")
    }

    IconImage {
        objectName: "emptyVault"
        visible: !emptyList.searching && !emptyList.filter.startsWith("audit:")
        source: "qrc:/icons/empty-vault.svg"
        color: advice.palette.text
        opacity: 0.6
        sourceSize: Qt.size(Math.round(150 * Appearance.factor), Math.round(130 * Appearance.factor))
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: 12 * Appearance.factor
    }
    Label {
        objectName: "emptyHeadline"
        text: emptyList.headline
        textFormat: Text.PlainText
        font.bold: !emptyList.searching
        opacity: 0.8
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
    }
    Label {
        id: advice
        objectName: "emptyAdvice"
        visible: !emptyList.searching && emptyList.forNewItems
        text: qsTr("Use “New Item” to add the first one.")
        opacity: 0.7
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
    }
    Button {
        objectName: "emptyAuditButton"
        visible: !emptyList.searching && (emptyList.onlineState === "failed"
                                          || (emptyList.onlineState === "off" && !VaultController.checkLeaks))
        text: emptyList.onlineState === "failed" ? qsTr("Check now") : qsTr("Settings…")
        onClicked: emptyList.onlineState === "failed" ? VaultController.checkLeaksNow() : emptyList.settingsRequested()
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 8 * Appearance.factor
    }
    AuditSource {
        visible: !emptyList.searching && emptyList.onlineState === "done"
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 8 * Appearance.factor
    }
}
