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
# qml/CMakeLists.txt 0.0.6

# --- Version history ----------------------------------------------------
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
        GeneratorDialog.qml
        DialogWindow.qml
        ItemEditor.qml
        Main.qml
        PasswordField.qml
        SettingsWindow.qml
        UnlockPage.qml
        UpdateWindow.qml
        VaultPage.qml
        WelcomePage.qml
    SOURCES
        ../src/ui/app_setup.cpp
        ../src/ui/app_setup.h
        ../src/ui/autostart.cpp
        ../src/ui/autostart.h
        ../src/ui/item_icons.cpp
        ../src/ui/item_icons.h
        ../src/ui/item_list_model.cpp
        ../src/ui/item_list_model.h
        ../src/ui/single_instance.cpp
        ../src/ui/single_instance.h
        ../src/ui/system_tray.cpp
        ../src/ui/system_tray.h
        ../src/ui/updater.cpp
        ../src/ui/updater.h
        ../src/ui/vault_controller.cpp
        ../src/ui/vault_controller.h
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
    PUBLIC chrono_vault_core Qt6::Gui Qt6::Qml Qt6::Widgets Qt6::Svg Qt6::Network
    PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent
)
# Strict for our own sources; Qt's generated QML cache is Qt's code.
if(LINUX)
    target_link_libraries(chrono_vault_ui PRIVATE Qt6::DBus)
endif()
chrono_vault_strict_warnings_for_sources(
    ../src/ui/app_setup.cpp
    ../src/ui/autostart.cpp
    ../src/ui/item_icons.cpp
    ../src/ui/item_list_model.cpp
    ../src/ui/single_instance.cpp
    ../src/ui/system_tray.cpp
    ../src/ui/updater.cpp
    ../src/ui/vault_controller.cpp
    ../src/ui/window_memory.cpp
)
