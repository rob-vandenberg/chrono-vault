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
// AuditSource.qml 0.6.86

// --- Version history ----------------------------------------------------
// v0.6.86: Initial release (D-065)

// Who the leaks and the breached websites are known from, with the link its
// licence asks for (CC BY 4.0).
Label {
    id: auditSource
    objectName: "auditSource"
    // The colour in the text itself: Qt keeps a link's first colour when the
    // theme changes.
    readonly property string source: "<a style=\"color:" + palette.link
                                     + "\" href=\"https://haveibeenpwned.com/\">Have I Been Pwned</a>"
    text: qsTr("From %1").arg(source)
    textFormat: Text.RichText
    opacity: 0.8
    onLinkActivated: function(address) { Qt.openUrlExternally(address) }
    HoverHandler { cursorShape: auditSource.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor }
}
