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
# src/tools/CMakeLists.txt 0.0.4

# --- Version history ----------------------------------------------------
# v0.0.4: Initial release

# The build's own tool, never shipped: the program's icon in every size each
# system wants, made from art/chrono-vault.svg (D-019).
qt_add_executable(make_icons make_icons.cpp)
target_link_libraries(make_icons PRIVATE Qt6::Gui Qt6::Svg)
chrono_vault_strict_warnings(make_icons)

set(CHRONO_VAULT_ICONS_DIR "${PROJECT_BINARY_DIR}/icons" CACHE INTERNAL "")
add_custom_command(
    OUTPUT "${CHRONO_VAULT_ICONS_DIR}/chrono-vault.ico" "${CHRONO_VAULT_ICONS_DIR}/chrono-vault-256.png"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CHRONO_VAULT_ICONS_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
            $<TARGET_FILE:make_icons> "${PROJECT_SOURCE_DIR}/art/chrono-vault.svg" "${CHRONO_VAULT_ICONS_DIR}"
    DEPENDS make_icons "${PROJECT_SOURCE_DIR}/art/chrono-vault.svg"
    COMMENT "Making the program's icon in every size from art/chrono-vault.svg"
    VERBATIM
)
add_custom_target(chrono_vault_icons ALL
    DEPENDS "${CHRONO_VAULT_ICONS_DIR}/chrono-vault.ico" "${CHRONO_VAULT_ICONS_DIR}/chrono-vault-256.png")
