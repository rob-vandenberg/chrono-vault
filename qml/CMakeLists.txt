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
# qml/CMakeLists.txt 0.3.66

# --- Version history ----------------------------------------------------
# v0.4.72: crypt32 for Windows Hello's key (D-038 point 11)
# v0.4.70: PIN unlock (D-059)
# v0.3.63: WebDavFileDialog.qml and the folder icon (D-057)
# v0.3.62: StorageWindow.qml and the two storage icons (D-057)
# v0.3.60: WebDAV: the client, the sync, the system store for its password, its windows (D-057)
# v0.2.56: CategoryEditor.qml (D-056)
# v0.0.40: IconPicker.qml, icon_library (D-050)
# v0.0.38: settings_files (D-026)
# v0.0.23: Unlocking with a fingerprint, a part per system (D-038)
# v0.0.22: The export window (D-037)
# v0.0.19: The import window (D-035)
# v0.0.17: A field's row in the item's view, a file of its own (D-033)
# v0.0.15: The window that changes the master password or keyfile (D-031)
# v0.0.14: The Customize page and the template editor; the categories', tags', favourites' and attachments' icons (D-027 to D-029)
# v0.0.10: Chrono Vault's own style for the UI scaling; every source of the library under the strict warnings (D-023)
# v0.0.9: The update windows of Chrono Mail in place of the one update window (D-024); the top bar's icons (D-025)
# v0.0.8: The licences, carried inside the program for the About box (D-021)
# v0.0.7: The theme, the UI scaling and the appearance the windows see (D-023)
# v0.0.6: The OK and Cancel icons
# v0.0.5: The About and Update windows
# v0.0.4: The style, the program's icon and our own icons from art/, the Noto fonts, Qt Svg and Qt Widgets
# v0.0.3: The pages, the editor, the generator, the settings window and the controller
# v0.0.2: The module's output folder is named after its URI, as Qt's tools expect
# v0.0.1: Initial release

# The windows, in Qt Quick (D-003), as one QML module that the program and
# the integration tests both load (D-012).
qt_add_library(chrono_vault_ui STATIC)
qt_add_qml_module(chrono_vault_ui
    URI ChronoVault
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ChronoVault"
    QML_FILES
        AboutWindow.qml
        ExportWindow.qml
        FieldRow.qml
        FilePicker.qml
        GeneratorDialog.qml
        IconPicker.qml
        CategoryEditor.qml
        ImportWindow.qml
        DialogWindow.qml
        ItemEditor.qml
        CustomizePage.qml
        ItemDetails.qml
        Main.qml
        MasterPasswordWindow.qml
        PasswordField.qml
        SettingsWindow.qml
        TemplateEditor.qml
        UnlockPage.qml
        UpdateAvailableWindow.qml
        UpdateDownloadedWindow.qml
        UpdatedWindow.qml
        VaultPage.qml
        WelcomePage.qml
        WebDavWindow.qml
        StorageWindow.qml
        SyncConflictsWindow.qml
        TickBox.qml
        ConflictWindow.qml
        BigButton.qml
        WebDavFileDialog.qml
    SOURCES
        ../src/ui/app_setup.cpp
        ../src/ui/app_setup.h
        ../src/ui/appearance.cpp
        ../src/ui/appearance.h
        ../src/ui/autostart.cpp
        ../src/ui/file_pickers.cpp
        ../src/ui/file_pickers.h
        ../src/ui/places_here.cpp
        ../src/ui/places_here.h
        ../src/ui/autostart.h
        ../src/ui/icon_library.cpp
        ../src/ui/icon_library.h
        ../src/ui/item_icons.cpp
        ../src/ui/item_icons.h
        ../src/ui/item_list_model.cpp
        ../src/ui/item_list_model.h
        ../src/ui/licences.cpp
        ../src/ui/licences.h
        ../src/ui/program_scale.cpp
        ../src/ui/quick_unlock.cpp
        ../src/ui/quick_unlock.h
        ../src/ui/system_activity.h
        ../src/ui/program_scale.h
        ../src/ui/settings_files.cpp
        ../src/ui/settings_files.h
        ../src/ui/single_instance.cpp
        ../src/ui/single_instance.h
        ../src/ui/system_tray.cpp
        ../src/ui/system_tray.h
        ../src/ui/theme.cpp
        ../src/ui/theme.h
        ../src/ui/updater.cpp
        ../src/ui/updater.h
        ../src/ui/vault_controller.cpp
        ../src/ui/vault_controller.h
        ../src/ui/vault_controller_pin.cpp
        ../src/ui/vault_controller_webdav.cpp
        ../src/ui/vault_controller_vaults.cpp
        ../src/ui/vault_controller_conflicts.cpp
        ../src/ui/vault_controller_import.cpp
        ../src/ui/secret_store.h
        ../src/ui/webdav_client.cpp
        ../src/ui/webdav_client.h
        ../src/ui/webdav_sync.cpp
        ../src/ui/webdav_sync.h
        ../src/ui/window_memory.cpp
        ../src/ui/window_memory.h
)
# The images the windows show come straight from art/ (only those are in git).
qt_add_resources(chrono_vault_ui chrono_vault_icons
    PREFIX "/icons"
    BASE "${PROJECT_SOURCE_DIR}/art"
    FILES
        "${PROJECT_SOURCE_DIR}/art/chrono-vault.svg"
        "${PROJECT_SOURCE_DIR}/art/eye.svg"
        "${PROJECT_SOURCE_DIR}/art/eye-hidden.svg"
        "${PROJECT_SOURCE_DIR}/art/item-login.svg"
        "${PROJECT_SOURCE_DIR}/art/item-note.svg"
        "${PROJECT_SOURCE_DIR}/art/ok.svg"
        "${PROJECT_SOURCE_DIR}/art/cancel.svg"
        "${PROJECT_SOURCE_DIR}/art/plus.svg"
        "${PROJECT_SOURCE_DIR}/art/generate.svg"
        "${PROJECT_SOURCE_DIR}/art/lock.svg"
        "${PROJECT_SOURCE_DIR}/art/settings.svg"
        "${PROJECT_SOURCE_DIR}/art/home.svg"
        "${PROJECT_SOURCE_DIR}/art/edit.svg"
        "${PROJECT_SOURCE_DIR}/art/item-card.svg"
        "${PROJECT_SOURCE_DIR}/art/item-identity.svg"
        "${PROJECT_SOURCE_DIR}/art/item-password.svg"
        "${PROJECT_SOURCE_DIR}/art/item-finance.svg"
        "${PROJECT_SOURCE_DIR}/art/item-computer.svg"
        "${PROJECT_SOURCE_DIR}/art/item-misc.svg"
        "${PROJECT_SOURCE_DIR}/art/tag.svg"
        "${PROJECT_SOURCE_DIR}/art/star.svg"
        "${PROJECT_SOURCE_DIR}/art/star-filled.svg"
        "${PROJECT_SOURCE_DIR}/art/attachment.svg"
        "${PROJECT_SOURCE_DIR}/art/copy.svg"
        "${PROJECT_SOURCE_DIR}/art/sidebar.svg"
        "${PROJECT_SOURCE_DIR}/art/archive.svg"
        "${PROJECT_SOURCE_DIR}/art/trash.svg"
        "${PROJECT_SOURCE_DIR}/art/storage-webdav.svg"
        "${PROJECT_SOURCE_DIR}/art/storage-device.svg"
        "${PROJECT_SOURCE_DIR}/art/folder.svg"
)
# The licences Chrono Vault ships, shown in the About box (D-021).
set_source_files_properties("${PROJECT_SOURCE_DIR}/LICENSE" PROPERTIES QT_RESOURCE_ALIAS "AGPL-3.0.txt")
set_source_files_properties("${PROJECT_SOURCE_DIR}/packaging/licences/LGPL-3.0.txt" PROPERTIES QT_RESOURCE_ALIAS "LGPL-3.0.txt")
set_source_files_properties("${PROJECT_SOURCE_DIR}/packaging/licences/GPL-3.0.txt" PROPERTIES QT_RESOURCE_ALIAS "GPL-3.0.txt")
set_source_files_properties("${PROJECT_SOURCE_DIR}/packaging/licences/GPL-2.0.txt" PROPERTIES QT_RESOURCE_ALIAS "GPL-2.0.txt")
set_source_files_properties("${PROJECT_SOURCE_DIR}/packaging/licences/Apache-2.0.txt" PROPERTIES QT_RESOURCE_ALIAS "Apache-2.0.txt")
set_source_files_properties("${PROJECT_SOURCE_DIR}/fonts/OFL.txt" PROPERTIES QT_RESOURCE_ALIAS "OFL-1.1.txt")
qt_add_resources(chrono_vault_ui chrono_vault_licences
    PREFIX "/licences"
    FILES
        "${PROJECT_SOURCE_DIR}/LICENSE"
        "${PROJECT_SOURCE_DIR}/packaging/licences/LGPL-3.0.txt"
        "${PROJECT_SOURCE_DIR}/packaging/licences/GPL-3.0.txt"
        "${PROJECT_SOURCE_DIR}/packaging/licences/GPL-2.0.txt"
        "${PROJECT_SOURCE_DIR}/packaging/licences/Apache-2.0.txt"
        "${PROJECT_SOURCE_DIR}/fonts/OFL.txt"
)
# The fonts, Google's Noto under the SIL Open Font License (OR-2, the owner's
# exception), carried inside the program so every system shows the same.
qt_add_resources(chrono_vault_ui chrono_vault_fonts
    PREFIX "/fonts"
    BASE "${PROJECT_SOURCE_DIR}/fonts"
    FILES
        "${PROJECT_SOURCE_DIR}/fonts/NotoSans-Regular.ttf"
        "${PROJECT_SOURCE_DIR}/fonts/NotoSans-Bold.ttf"
        "${PROJECT_SOURCE_DIR}/fonts/NotoSansMono-Regular.ttf"
)
target_include_directories(chrono_vault_ui PUBLIC "${PROJECT_SOURCE_DIR}/src/ui")
target_link_libraries(chrono_vault_ui
    PUBLIC chrono_vault_core chrono_vault_style chrono_vault_styleplugin Qt6::Gui Qt6::Qml Qt6::Widgets Qt6::Svg Qt6::Network
    PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent
)
# Strict for our own sources; Qt's generated QML cache is Qt's code.
if(LINUX)
    target_link_libraries(chrono_vault_ui PRIVATE Qt6::DBus)
endif()
# Unlocking with a fingerprint (D-038): fprintd and the keyring on Linux,
# Windows Hello through C++/WinRT on Windows, none elsewhere yet.
# The system's store for the WebDAV password (D-057): the keyring on Linux,
# the Credential Manager on Windows, none elsewhere yet.
if(LINUX)
    target_sources(chrono_vault_ui PRIVATE ../src/ui/secret_store_linux.cpp)
elseif(WIN32)
    target_sources(chrono_vault_ui PRIVATE ../src/ui/secret_store_windows.cpp)
    target_link_libraries(chrono_vault_ui PRIVATE advapi32)
else()
    target_sources(chrono_vault_ui PRIVATE ../src/ui/secret_store_none.cpp)
endif()
if(LINUX)
    target_sources(chrono_vault_ui PRIVATE ../src/ui/quick_unlock_linux.cpp)
elseif(WIN32)
    target_sources(chrono_vault_ui PRIVATE ../src/ui/quick_unlock_windows.cpp)
    target_link_libraries(chrono_vault_ui PRIVATE windowsapp crypt32)
    set_source_files_properties(../src/ui/quick_unlock_windows.cpp PROPERTIES COMPILE_OPTIONS "/bigobj")
else()
    target_sources(chrono_vault_ui PRIVATE ../src/ui/quick_unlock_none.cpp)
endif()
# What the system says for the autolock (D-040): logind and the desktop over
# D-Bus on Linux, the power messages and the last input on Windows.
if(LINUX)
    target_sources(chrono_vault_ui PRIVATE ../src/ui/system_activity_linux.cpp)
elseif(WIN32)
    target_sources(chrono_vault_ui PRIVATE ../src/ui/system_activity_windows.cpp)
    target_link_libraries(chrono_vault_ui PRIVATE user32)
else()
    target_sources(chrono_vault_ui PRIVATE ../src/ui/system_activity_none.cpp)
endif()
chrono_vault_strict_warnings_for_sources(
    ../src/ui/app_setup.cpp
    ../src/ui/file_pickers.cpp
    ../src/ui/places_here.cpp
    ../src/ui/appearance.cpp
    ../src/ui/licences.cpp
    ../src/ui/program_scale.cpp
    ../src/ui/quick_unlock.cpp
    ../src/ui/settings_files.cpp
    ../src/ui/theme.cpp
    ../src/ui/autostart.cpp
    ../src/ui/icon_library.cpp
    ../src/ui/item_icons.cpp
    ../src/ui/item_list_model.cpp
    ../src/ui/single_instance.cpp
    ../src/ui/system_tray.cpp
    ../src/ui/updater.cpp
    ../src/ui/vault_controller.cpp
    ../src/ui/vault_controller_pin.cpp
    ../src/ui/vault_controller_webdav.cpp
    ../src/ui/vault_controller_vaults.cpp
    ../src/ui/vault_controller_conflicts.cpp
    ../src/ui/vault_controller_import.cpp
    ../src/ui/webdav_client.cpp
    ../src/ui/webdav_sync.cpp
    ../src/ui/window_memory.cpp
)
