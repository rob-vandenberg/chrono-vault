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
# qml/CMakeLists.txt 0.0.3

# --- Version history ----------------------------------------------------
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
        GeneratorDialog.qml
        ItemEditor.qml
        Main.qml
        SettingsWindow.qml
        UnlockPage.qml
        VaultPage.qml
        WelcomePage.qml
    SOURCES
        ../src/ui/item_list_model.cpp
        ../src/ui/item_list_model.h
        ../src/ui/vault_controller.cpp
        ../src/ui/vault_controller.h
)
target_include_directories(chrono_vault_ui PUBLIC "${PROJECT_SOURCE_DIR}/src/ui")
target_link_libraries(chrono_vault_ui
    PUBLIC chrono_vault_core Qt6::Gui Qt6::Qml
    PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent
)
# Strict for our own sources; Qt's generated QML cache is Qt's code.
chrono_vault_strict_warnings_for_sources(
    ../src/ui/item_list_model.cpp
    ../src/ui/vault_controller.cpp
)
