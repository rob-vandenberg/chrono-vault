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
# tests/integration/CMakeLists.txt 0.0.5

# --- Version history ----------------------------------------------------
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
