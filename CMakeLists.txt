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

cmake_minimum_required(VERSION 3.21)

project(chrono-vault VERSION 0.0.3 LANGUAGES CXX)

# --- Version ------------------------------------------------------------
# CMakeLists.txt 0.0.3

# --- Version history ----------------------------------------------------
# v0.0.3: Qt Concurrent, for the slow steps off the window's thread (OR-5)
# v0.0.2: Translations with Poedit (D-008); OpenSSL 3.2 or newer, for Argon2id and AES-256-GCM (D-013)
# v0.0.1: Initial release

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHRONO_VAULT_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Concurrent Test LinguistTools)
qt_standard_project_setup(REQUIRES 6.8)
find_package(OpenSSL 3.2 REQUIRED COMPONENTS Crypto)

include(cmake/ChronoVaultBuild.cmake)
include(cmake/ChronoVaultTranslations.cmake)

add_subdirectory(src/core)
add_subdirectory(qml)
add_subdirectory(src/app)

enable_testing()
add_subdirectory(tests)
