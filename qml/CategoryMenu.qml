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
// CategoryMenu.qml 0.5.83

// --- Version history ----------------------------------------------------
// v0.5.83: Initial release (D-063)

// Change Category: the categories the left column shows, for the selected
// items; in the Item menu and in an item's right-click menu.
Menu {
    id: categoryMenu
    title: qsTr("Change Category")
    signal chosen(string categoryId)
    Instantiator {
        model: VaultController.revision >= 0
               ? VaultController.categories().filter(function(category) { return !category.hidden }) : []
        delegate: MenuItem {
            required property var modelData
            objectName: "toCategory_" + modelData.id
            text: modelData.name
            icon.source: modelData.icon
            icon.color: "transparent"
            onTriggered: categoryMenu.chosen(modelData.id)
        }
        onObjectAdded: function(index, object) { categoryMenu.insertItem(index, object) }
        onObjectRemoved: function(index, object) { categoryMenu.removeItem(object) }
    }
}
