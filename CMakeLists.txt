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

project(chrono-vault VERSION 0.5.82 LANGUAGES CXX)

# --- Version ------------------------------------------------------------
# CMakeLists.txt 0.5.82

# --- Version history ----------------------------------------------------
# v0.5.82: Version 0.5.82
# v0.5.81: Version 0.5.81
# v0.5.80: Version 0.5.80
# v0.5.79: Version 0.5.79
# v0.5.78.3: Debug version 0.5.78.3
# v0.5.78.2: Debug version 0.5.78.2
# v0.5.78.1: Debug version 0.5.78.1
# v0.5.78: Version 0.5.78
# v0.5.77: Version 0.5.77
# v0.5.76: Version 0.5.76
# v0.5.75: Version 0.5.75
# v0.4.74: Version 0.4.74
# v0.4.73: Version 0.4.73
# v0.4.72.1: Debug version: Windows Hello's window in the --debug log
# v0.4.72: Version 0.4.72
# v0.4.71: Version 0.4.71
# v0.4.70: Version 0.4.70
# v0.3.67: Version 0.3.67
# v0.3.66: Version 0.3.66
# v0.3.65: Version 0.3.65
# v0.3.64: Version 0.3.64
# v0.3.63: Version 0.3.63
# v0.3.62: Version 0.3.62
# v0.3.61: Version 0.3.61
# v0.3.60: Version 0.3.60: WebDAV storage (D-057)
# v0.2.59: Version 0.2.59
# v0.2.58: Version 0.2.58
# v0.2.57: Version 0.2.57
# v0.2.56: Version 0.2.56
# v0.2.55: Version 0.2.55
# v0.2.54: Version 0.2.54
# v0.2.53: Version 0.2.53
# v0.2.52: Version 0.2.52
# v0.2.51: Version 0.2.51
# v0.2.50: Version 0.2.50
# v0.2.49: Version 0.2.49
# v0.2.48: Version 0.2.48
# v0.2.47: Version 0.2.47
# v0.2.46: Version 0.2.46
# v0.2.45: Version 0.2.45, merging (D-054)
# v0.1.44: Version 0.1.44
# v0.1.43: Version 0.1.43
# v0.1.42: Version 0.1.42
# v0.1.41: Version 0.1.41
# v0.0.40: Version 0.0.40
# v0.0.39: Version 0.0.39
# v0.0.38: Version 0.0.38
# v0.0.37: Version 0.0.37
# v0.0.36: Version 0.0.36
# v0.0.35: Version 0.0.35
# v0.0.34: Version 0.0.34
# v0.0.33: Version 0.0.33
# v0.0.32: Version 0.0.32
# v0.0.31: Version 0.0.31
# v0.0.30: Version 0.0.30
# v0.0.29: Version 0.0.29
# v0.0.28: Version 0.0.28
# v0.0.27: Version 0.0.27
# v0.0.26: Version 0.0.26
# v0.0.25: Version 0.0.25
# v0.0.24: Version 0.0.24
# v0.0.23: Version 0.0.23
# v0.0.22: Version 0.0.22
# v0.0.21: Version 0.0.21
# v0.0.20: Version 0.0.20
# v0.0.19: Version 0.0.19
# v0.0.18: Version 0.0.18
# v0.0.17: Version 0.0.17
# v0.0.16: Version 0.0.16
# v0.0.15: Version 0.0.15
# v0.0.14: Version 0.0.14
# v0.0.13: Version 0.0.13
# v0.0.12: Version 0.0.12
# v0.0.11: Version 0.0.11
# v0.0.10: Version 0.0.10; Chrono Vault's own style (D-023)
# v0.0.9: Version 0.0.9
# v0.0.8: Version 0.0.8
# v0.0.7: Version 0.0.7
# v0.0.6: Version 0.0.6
# v0.0.5: Version 0.0.5; Qt DBus on Linux, for installing an update through PackageKit (D-021)
# v0.0.4: Qt Widgets (the tray) and Qt Svg (the icons); the icon tool; tests can be left out of a release
# v0.0.3: Qt Concurrent, for the slow steps off the window's thread (OR-5)
# v0.0.2: Translations with Poedit (D-008); OpenSSL 3.2 or newer, for Argon2id and AES-256-GCM (D-013)
# v0.0.1: Initial release

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHRONO_VAULT_BUILD_TESTS "Build the tests (a release build leaves them out)" ON)
option(CHRONO_VAULT_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Svg Network Qml Quick QuickControls2 Concurrent Test LinguistTools)
qt_standard_project_setup(REQUIRES 6.8)
find_package(OpenSSL 3.2 REQUIRED COMPONENTS Crypto)
if(LINUX)
    # PackageKit, which installs an update on Linux (D-021).
    find_package(Qt6 REQUIRED COMPONENTS DBus)
endif()

include(cmake/ChronoVaultBuild.cmake)
include(cmake/ChronoVaultTranslations.cmake)

add_subdirectory(src/tools)
add_subdirectory(src/core)
add_subdirectory(style)
add_subdirectory(qml)
add_subdirectory(src/app)

if(CHRONO_VAULT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
