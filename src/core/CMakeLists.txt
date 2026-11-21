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
# src/core/CMakeLists.txt 0.0.5

# --- Version history ----------------------------------------------------
# v0.0.5: The release check (D-021)
# v0.0.2: Encryption with OpenSSL (D-013), the vault file (D-014), the contents (D-015), the password generator and one-time codes
# v0.0.1: Initial release

# The vault core: plain C++ with Qt Core only, no window code (D-012).
qt_add_library(chrono_vault_core STATIC
    crypto.cpp
    crypto.h
    password_generator.cpp
    password_generator.h
    release_check.cpp
    release_check.h
    totp.cpp
    totp.h
    vault_contents.cpp
    vault_contents.h
    vault_file.cpp
    vault_file.h
    version.cpp
    version.h
)
target_include_directories(chrono_vault_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chrono_vault_core PUBLIC Qt6::Core PRIVATE OpenSSL::Crypto)
# The one place the version lives is the project() line (CPP-2).
target_compile_definitions(chrono_vault_core PRIVATE CHRONO_VAULT_VERSION="${PROJECT_VERSION}")
chrono_vault_strict_warnings(chrono_vault_core)
