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
# tests/integration/CMakeLists.txt 0.3.62

# --- Version history ----------------------------------------------------
# v0.4.70: PIN unlock (D-059)
# v0.3.62: test_webdav_create (D-057)
# v0.3.60: The WebDAV sync and windows tests, with our own test server (D-057)
# v0.0.40: test_icon_picker (D-050)
# v0.0.23: The fingerprint unlock test, with a stand-in
# v0.0.22: The item pages test, apart from the pages test (OR-11)
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

qt_add_executable(test_item_pages test_item_pages.cpp)
target_link_libraries(test_item_pages PRIVATE chrono_vault_ui chrono_vault_uiplugin Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::Test)
chrono_vault_strict_warnings(test_item_pages)
chrono_vault_add_test(test_item_pages)

qt_add_executable(test_file_pickers test_file_pickers.cpp)
target_link_libraries(test_file_pickers PRIVATE chrono_vault_ui chrono_vault_uiplugin Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::Test)
chrono_vault_strict_warnings(test_file_pickers)
chrono_vault_add_test(test_file_pickers)

qt_add_executable(test_menus test_menus.cpp)
target_link_libraries(test_menus PRIVATE chrono_vault_ui chrono_vault_uiplugin Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
chrono_vault_strict_warnings(test_menus)
chrono_vault_add_test(test_menus)

qt_add_executable(test_autolock test_autolock.cpp)
target_link_libraries(test_autolock PRIVATE chrono_vault_ui chrono_vault_uiplugin Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
chrono_vault_strict_warnings(test_autolock)
chrono_vault_add_test(test_autolock)

qt_add_executable(test_unlock_waits test_unlock_waits.cpp)
target_link_libraries(test_unlock_waits PRIVATE chrono_vault_ui chrono_vault_uiplugin Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
chrono_vault_strict_warnings(test_unlock_waits)
chrono_vault_add_test(test_unlock_waits)

qt_add_executable(test_pin_unlock test_pin_unlock.cpp)
target_link_libraries(test_pin_unlock PRIVATE chrono_vault_ui chrono_vault_uiplugin Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
chrono_vault_strict_warnings(test_pin_unlock)
chrono_vault_add_test(test_pin_unlock)

qt_add_executable(test_reader test_reader.cpp)
target_link_libraries(test_reader PRIVATE chrono_vault_ui chrono_vault_uiplugin Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
chrono_vault_strict_warnings(test_reader)
chrono_vault_add_test(test_reader)

qt_add_executable(test_settings_place test_settings_place.cpp)
target_link_libraries(test_settings_place PRIVATE chrono_vault_ui Qt6::Gui Qt6::Test)
chrono_vault_strict_warnings(test_settings_place)
chrono_vault_add_test(test_settings_place)

qt_add_executable(test_quick_unlock test_quick_unlock.cpp)
target_link_libraries(test_quick_unlock PRIVATE chrono_vault_ui Qt6::Gui Qt6::Test)
chrono_vault_strict_warnings(test_quick_unlock)
chrono_vault_add_test(test_quick_unlock)

qt_add_executable(test_icon_picker test_icon_picker.cpp)
target_link_libraries(test_icon_picker PRIVATE chrono_vault_ui chrono_vault_uiplugin Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
chrono_vault_strict_warnings(test_icon_picker)
chrono_vault_add_test(test_icon_picker)

# A vault kept on a WebDAV server: two devices, a server of our own (D-057).
qt_add_executable(test_webdav_sync test_webdav_sync.cpp ../webdav_test_server.cpp ../webdav_test_server.h)
target_include_directories(test_webdav_sync PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(test_webdav_sync PRIVATE chrono_vault_ui Qt6::Gui Qt6::Network Qt6::Test)
chrono_vault_strict_warnings(test_webdav_sync)
chrono_vault_add_test(test_webdav_sync)

# The same test's interruptions at every step, apart (OR-11: under 20 seconds).
qt_add_executable(test_webdav_interruptions test_webdav_sync.cpp ../webdav_test_server.cpp ../webdav_test_server.h)
target_compile_definitions(test_webdav_interruptions PRIVATE CHRONO_VAULT_WEBDAV_INTERRUPTIONS)
target_include_directories(test_webdav_interruptions PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(test_webdav_interruptions PRIVATE chrono_vault_ui Qt6::Gui Qt6::Network Qt6::Test)
chrono_vault_strict_warnings(test_webdav_interruptions)
chrono_vault_add_test(test_webdav_interruptions)

# And its tests of making a vault on the server, apart (OR-11).
qt_add_executable(test_webdav_create test_webdav_sync.cpp ../webdav_test_server.cpp ../webdav_test_server.h)
target_compile_definitions(test_webdav_create PRIVATE CHRONO_VAULT_WEBDAV_CREATE)
target_include_directories(test_webdav_create PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(test_webdav_create PRIVATE chrono_vault_ui Qt6::Gui Qt6::Network Qt6::Test)
chrono_vault_strict_warnings(test_webdav_create)
chrono_vault_add_test(test_webdav_create)

qt_add_executable(test_webdav_windows test_webdav_windows.cpp ../webdav_test_server.cpp ../webdav_test_server.h)
target_include_directories(test_webdav_windows PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(test_webdav_windows PRIVATE
    chrono_vault_ui chrono_vault_uiplugin Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network Qt6::Test)
chrono_vault_strict_warnings(test_webdav_windows)
chrono_vault_add_test(test_webdav_windows)
