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
# ChronoVaultTranslations.cmake 0.0.2

# --- Version history ----------------------------------------------------
# v0.0.2: Initial release

# Translations with Poedit (D-008). US English is written in the code; every
# other language is a .po file in translations/, made in Poedit from the
# template translations/chrono-vault.pot.
#
#   cmake --build build --target update_translations
#       gathers every text of the code and the windows into
#       translations/chrono-vault.ts (Qt's own format) and from that writes
#       translations/chrono-vault.pot, the template Poedit opens.
#
# Every translations/chrono-vault_<language>.po is compiled at build time and
# carried inside the program, under :/i18n.

set(CHRONO_VAULT_TRANSLATIONS_DIR "${PROJECT_SOURCE_DIR}/translations")

add_custom_target(update_translations
    COMMAND Qt6::lupdate -silent -locations relative -extensions cpp,h,qml
            "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/qml"
            -ts "${CHRONO_VAULT_TRANSLATIONS_DIR}/chrono-vault.ts"
    COMMAND Qt6::lconvert -i "${CHRONO_VAULT_TRANSLATIONS_DIR}/chrono-vault.ts"
            -o "${CHRONO_VAULT_TRANSLATIONS_DIR}/chrono-vault.pot" -of pot
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Gathering the texts into translations/chrono-vault.ts and chrono-vault.pot"
    VERBATIM
)

# Compiles every .po into a .qm and carries them inside the given target.
function(chrono_vault_add_translations target)
    file(GLOB po_files CONFIGURE_DEPENDS "${CHRONO_VAULT_TRANSLATIONS_DIR}/chrono-vault_*.po")
    set(qm_files "")
    foreach(po_file IN LISTS po_files)
        get_filename_component(name "${po_file}" NAME_WE)
        set(qm_file "${CMAKE_CURRENT_BINARY_DIR}/i18n/${name}.qm")
        add_custom_command(
            OUTPUT "${qm_file}"
            COMMAND Qt6::lconvert -i "${po_file}" -o "${qm_file}" -of qm
            DEPENDS "${po_file}"
            VERBATIM
        )
        list(APPEND qm_files "${qm_file}")
    endforeach()
    if(qm_files)
        qt_add_resources(${target} chrono_vault_translations
            PREFIX "/i18n"
            BASE "${CMAKE_CURRENT_BINARY_DIR}/i18n"
            FILES ${qm_files}
        )
    endif()
endfunction()
