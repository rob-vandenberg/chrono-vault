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
# tests/unit/CMakeLists.txt 0.0.2

# --- Version history ----------------------------------------------------
# v0.0.2: The tests of translatable texts, the encryption, the vault file, the contents, the generator and one-time codes
# v0.0.1: Initial release

qt_add_executable(test_version test_version.cpp)
target_link_libraries(test_version PRIVATE chrono_vault_core Qt6::Test)
target_compile_definitions(test_version PRIVATE CHRONO_VAULT_PROJECT_VERSION="${PROJECT_VERSION}")
chrono_vault_strict_warnings(test_version)
chrono_vault_add_test(test_version)

qt_add_executable(test_every_string_translatable test_every_string_translatable.cpp)
target_link_libraries(test_every_string_translatable PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(test_every_string_translatable PRIVATE CHRONO_VAULT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
chrono_vault_strict_warnings(test_every_string_translatable)
chrono_vault_add_test(test_every_string_translatable)

# The tests of one part of the vault core each.
foreach(core_test IN ITEMS test_crypto test_vault_file test_vault_contents test_password_generator test_totp)
    qt_add_executable(${core_test} ${core_test}.cpp)
    target_link_libraries(${core_test} PRIVATE chrono_vault_core Qt6::Test)
    chrono_vault_strict_warnings(${core_test})
    chrono_vault_add_test(${core_test})
endforeach()
