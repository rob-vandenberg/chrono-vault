# SPDX-License-Identifier: AGPL-3.0-only
#
# Chrono Vault, a password manager.
# Copyright (C) 2026 Rob Vandenberg <RobVandenberg64@gmail.com>
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU Affero General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
# General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

# --- Version ------------------------------------------------------------
# tests/integration/CMakeLists.txt 0.0.20

# --- Version history ----------------------------------------------------
# v0.0.20: The website icons test
# v0.0.17: The items test: history, sections, hidden fields, Trash and Archive
# v0.0.16: The test of a change made elsewhere
# v0.0.15: The vaults test: several vaults, the keyfile, changing the master password
# v0.0.14: The customizing test: categories, templates, tags, favourites, attachments
# v0.0.9: The update windows test
# v0.0.7: The theme and UI scaling test
# v0.0.5: The update test
# v0.0.4: The window test uses the UI library, for the style; the tray and icon tests
# v0.0.3: The tests of the controller and of the pages
# v0.0.1: Initial release

qt_add_executable(test_main_window test_main_window.cpp)
target_link_libraries(test_main_window PRIVATE
    chrono_vault_ui
    chrono_vault_uiplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
chrono_vault_strict_warnings(test_main_window)
chrono_vault_add_test(test_main_window)

qt_add_executable(test_vault_controller test_vault_controller.cpp)
target_link_libraries(test_vault_controller PRIVATE chrono_vault_ui Qt6::Gui Qt6::Test)
chrono_vault_strict_warnings(test_vault_controller)
chrono_vault_add_test(test_vault_controller)

qt_add_executable(test_pages test_pages.cpp)
target_link_libraries(test_pages PRIVATE chrono_vault_ui chrono_vault_uiplugin Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
chrono_vault_strict_warnings(test_pages)
chrono_vault_add_test(test_pages)

qt_add_executable(test_tray test_tray.cpp)
target_link_libraries(test_tray PRIVATE chrono_vault_ui chrono_vault_uiplugin Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::Test)
chrono_vault_strict_warnings(test_tray)
chrono_vault_add_test(test_tray)

qt_add_executable(test_item_icons test_item_icons.cpp)
target_link_libraries(test_item_icons PRIVATE chrono_vault_ui Qt6::Gui Qt6::Test)
chrono_vault_strict_warnings(test_item_icons)
chrono_vault_add_test(test_item_icons)

qt_add_executable(test_windows test_windows.cpp)
target_link_libraries(test_windows PRIVATE chrono_vault_ui Qt6::Gui Qt6::Network Qt6::Test)
chrono_vault_strict_warnings(test_windows)
chrono_vault_add_test(test_windows)

qt_add_executable(test_updater test_updater.cpp)
target_link_libraries(test_updater PRIVATE chrono_vault_ui Qt6::Network Qt6::Test)
chrono_vault_strict_warnings(test_updater)
chrono_vault_add_test(test_updater)

qt_add_executable(test_appearance test_appearance.cpp)
target_link_libraries(test_appearance PRIVATE chrono_vault_ui chrono_vault_uiplugin Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::Test)
chrono_vault_strict_warnings(test_appearance)
chrono_vault_add_test(test_appearance)

qt_add_executable(test_update_windows test_update_windows.cpp)
target_link_libraries(test_update_windows PRIVATE chrono_vault_ui chrono_vault_uiplugin Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::Network Qt6::Test)
chrono_vault_strict_warnings(test_update_windows)
chrono_vault_add_test(test_update_windows)

qt_add_executable(test_customizing test_customizing.cpp)
target_link_libraries(test_customizing PRIVATE chrono_vault_ui Qt6::Gui Qt6::Test)
chrono_vault_strict_warnings(test_customizing)
chrono_vault_add_test(test_customizing)

qt_add_executable(test_vaults test_vaults.cpp)
target_link_libraries(test_vaults PRIVATE chrono_vault_ui Qt6::Gui Qt6::Test)
chrono_vault_strict_warnings(test_vaults)
chrono_vault_add_test(test_vaults)

qt_add_executable(test_elsewhere test_elsewhere.cpp)
target_link_libraries(test_elsewhere PRIVATE chrono_vault_ui Qt6::Gui Qt6::Test)
chrono_vault_strict_warnings(test_elsewhere)
chrono_vault_add_test(test_elsewhere)

qt_add_executable(test_items test_items.cpp)
target_link_libraries(test_items PRIVATE chrono_vault_ui Qt6::Gui Qt6::Test)
chrono_vault_strict_warnings(test_items)
chrono_vault_add_test(test_items)

qt_add_executable(test_website_icons test_website_icons.cpp)
target_link_libraries(test_website_icons PRIVATE chrono_vault_ui Qt6::Gui Qt6::Network Qt6::Test)
chrono_vault_strict_warnings(test_website_icons)
chrono_vault_add_test(test_website_icons)
