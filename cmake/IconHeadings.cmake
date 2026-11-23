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
# IconHeadings.cmake 0.2.56

# --- Version history ----------------------------------------------------
# v0.2.56: The icons' names too, from their file names (the owner, D-050)
# v0.0.40: Initial release (D-050)

# Run by update_translations: the headings of icons/icons.md ("folder:
# Heading", one per line) written as texts to translate, in the context
# "IconSections", and the names of the icons in those folders (their file
# names) in the context "IconNames", so they reach the .ts and .pot files like
# every other text. A line starting with "-" is a folder taken out.
#   cmake -DICONS_MD=<icons.md> -DOUT=<file.cpp> -P IconHeadings.cmake

set(lines "")
if(EXISTS "${ICONS_MD}")
    file(STRINGS "${ICONS_MD}" lines ENCODING UTF-8)
endif()
set(body "// Made by cmake/IconHeadings.cmake from icons/icons.md; never edited by hand.\n")
string(APPEND body "#include <QtGlobal>\n")
string(APPEND body "[[maybe_unused]] static const char* const iconHeadings[] = {\n")
get_filename_component(icons_dir "${ICONS_MD}" DIRECTORY)
foreach(line IN LISTS lines)
    string(STRIP "${line}" line)
    string(FIND "${line}" ":" colon)
    if(colon GREATER 0 AND NOT line MATCHES "^-")
        string(SUBSTRING "${line}" 0 ${colon} folder)
        string(STRIP "${folder}" folder)
        file(GLOB pictures LIST_DIRECTORIES false "${icons_dir}/${folder}/*")
        list(SORT pictures)
        foreach(picture IN LISTS pictures)
            get_filename_component(name "${picture}" NAME_WLE)
            string(REPLACE "\\" "\\\\" name "${name}")
            string(REPLACE "\"" "\\\"" name "${name}")
            string(APPEND body "    QT_TRANSLATE_NOOP(\"IconNames\", \"${name}\"),\n")
        endforeach()
        math(EXPR start "${colon} + 1")
        string(SUBSTRING "${line}" ${start} -1 heading)
        string(STRIP "${heading}" heading)
        if(NOT heading STREQUAL "")
            string(REPLACE "\\" "\\\\" heading "${heading}")
            string(REPLACE "\"" "\\\"" heading "${heading}")
            string(APPEND body "    QT_TRANSLATE_NOOP(\"IconSections\", \"${heading}\"),\n")
        endif()
    endif()
endforeach()
string(APPEND body "    nullptr};\n")
file(WRITE "${OUT}" "${body}")
