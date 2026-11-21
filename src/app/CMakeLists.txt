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
# src/app/CMakeLists.txt 0.0.2

# --- Version history ----------------------------------------------------
# v0.0.2: The translations are carried inside the program (D-008)
# v0.0.1: Initial release

qt_add_executable(chrono-vault
    main.cpp
)
target_link_libraries(chrono-vault PRIVATE
    chrono_vault_core
    chrono_vault_uiplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)
chrono_vault_strict_warnings(chrono-vault)
chrono_vault_add_translations(chrono-vault)
