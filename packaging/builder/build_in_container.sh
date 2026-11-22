#!/usr/bin/env bash
#
# SPDX-License-Identifier: AGPL-3.0-only
#
# Chrono Vault, a password manager.
# Copyright (C) 2026 Rob Vandenberg
#
# --- Version ------------------------------------------------------------
# build_in_container.sh 0.0.10
#
# --- Version history ----------------------------------------------------
# v0.0.10: The style folder, Chrono Vault's own Qt Quick style (D-023)
# v0.0.5: Qt's TLS plugins and OpenSSL's libssl, for HTTPS to GitHub (D-021)
# v0.0.4: Initial release, modelled on Chrono Viewer's
#
# Runs inside the builder image, started by build-release.sh. /src is the project, read only; /out receives the
# program folder; /cache keeps the compiler's cache between builds.
#
# It builds the program in Release against our own Qt and OpenSSL and makes the program folder: the program,
# qt.conf, lib/ with every library of Qt and OpenSSL it needs, plugins/ with Qt's plugins, qml/ with Qt's QML
# modules, icons/ made from art/chrono-vault.svg, and licences/ with the terms of everything that ships. Every
# library and plugin looks beside itself, and the folder is checked: nothing in it may need a library that is
# neither in it nor on this old system.
set -euo pipefail

QT=/opt/qt/6.10.2/gcc_64
SSL=/opt/openssl
OUT=/out/chrono-vault
export PATH="/opt/tools/bin:$PATH"
export CCACHE_DIR=/cache/ccache

stop() {
    echo "STOPPED: $1" >&2
    exit 1
}

# Only what a build needs is copied in, never the whole folder.
rm -rf /work
mkdir -p /work
tar -C /src -cf - CMakeLists.txt LICENSE cmake src qml style art fonts packaging translations | tar -C /work -xf -

cmake -S /work -B /work/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCHRONO_VAULT_BUILD_TESTS=OFF \
    -DCMAKE_PREFIX_PATH="$QT" -DOPENSSL_ROOT_DIR="$SSL" -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DCMAKE_INSTALL_PREFIX="$OUT" >/dev/null
cmake --build /work/build
rm -rf "$OUT"
cmake --install /work/build >/dev/null
cp /work/packaging/qt.conf "$OUT/qt.conf"
mkdir -p "$OUT/icons"
cp /work/build/icons/chrono-vault-*.png "$OUT/icons/"

# --- Qt's plugins: what Chrono Vault on X11 or Wayland needs -----------------------------------------------------
copy_plugins() {
    local kind="$1"
    shift
    mkdir -p "$OUT/plugins/$kind"
    local wanted
    for wanted in "$@"; do
        cp -a "$QT/plugins/$kind/"$wanted "$OUT/plugins/$kind/"
    done
}
copy_plugins platforms 'libqxcb.so' 'libqwayland*.so' 'libqoffscreen.so'
copy_plugins platforminputcontexts '*.so'
copy_plugins platformthemes 'libqxdgdesktopportal.so'
copy_plugins xcbglintegrations '*.so'
copy_plugins imageformats 'libqsvg.so' 'libqico.so' 'libqjpeg.so'
copy_plugins iconengines 'libqsvgicon.so'
copy_plugins tls '*.so'
for kind in "$QT"/plugins/wayland-*; do
    copy_plugins "$(basename "$kind")" '*.so'
done

# --- Qt's QML modules the windows load ---------------------------------------------------------------------------
mkdir -p "$OUT/qml"
cp -a "$QT/qml/QtQuick" "$QT/qml/QtQml" "$QT/qml/QtCore" "$OUT/qml/"

# --- Every library the program, its plugins and the QML modules need from our Qt or OpenSSL ----------------------
mkdir -p "$OUT/lib"
export LD_LIBRARY_PATH="$QT/lib:$SSL/lib"
gather() {
    local added=1 file needed name
    while [[ $added -eq 1 ]]; do
        added=0
        while IFS= read -r file; do
            while IFS= read -r needed; do
                name="$(basename "$needed")"
                if [[ "$needed" == "$QT"/lib/* || "$needed" == "$SSL"/lib/* ]]; then
                    if [[ ! -e "$OUT/lib/$name" ]]; then
                        cp -L "$needed" "$OUT/lib/$name"
                        added=1
                    fi
                fi
            done < <(ldd "$file" | awk '/=> \// { print $3 }')
        done < <(find "$OUT" -type f \( -name '*.so*' -o -name chrono-vault \))
    done
}
gather
unset LD_LIBRARY_PATH
# Qt loads OpenSSL's libssl while the program runs (for HTTPS, D-021), so no
# library names it: it is put beside the others by name.
cp -L "$SSL/lib/libssl.so.3" "$OUT/lib/libssl.so.3"

# Every library and plugin looks beside itself: lib/ for a library, the right number of folders up for a plugin.
for library in "$OUT"/lib/*.so*; do
    patchelf --set-rpath '$ORIGIN' "$library"
done
while IFS= read -r plugin; do
    relative="$(realpath --relative-to="$(dirname "$plugin")" "$OUT/lib")"
    patchelf --set-rpath "\$ORIGIN/$relative" "$plugin"
done < <(find "$OUT/plugins" "$OUT/qml" -type f -name '*.so')
patchelf --set-rpath '$ORIGIN/lib' "$OUT/chrono-vault"

MISSING="$(find "$OUT" -type f \( -name '*.so*' -o -name chrono-vault \) -exec ldd {} + 2>/dev/null | grep 'not found' | sort -u || true)"
[[ -z "$MISSING" ]] || stop "a library is missing from the program folder:
$MISSING"

# --- The licences: the terms of everything that ships, beside the program, in every package ---------------------
LICENCES="$OUT/licences"
rm -rf "$LICENCES"
licence() {
    local into="$1" file
    shift
    mkdir -p "$LICENCES/$into"
    for file in "$@"; do
        [[ -f "$file" ]] || stop "a licence text is missing: $file."
        cp -L "$file" "$LICENCES/$into/"
    done
}
licence qt /work/packaging/licences/LGPL-3.0.txt /work/packaging/licences/GPL-3.0.txt \
    /work/packaging/licences/GPL-2.0.txt "$QT"/sbom/*.spdx*
licence openssl "$SSL/licences/LICENSE.txt"
licence noto-fonts /work/fonts/OFL.txt
find "$LICENCES" -type f -exec chmod 0644 {} +

# The program starts here, with no screen, and keeps running: a first check before the packages.
QT_QPA_PLATFORM=offscreen timeout 5 "$OUT/chrono-vault" >/dev/null 2>&1 && RESULT=0 || RESULT=$?
[[ $RESULT -eq 124 ]] || stop "the built program stopped at once (exit $RESULT)."

echo "Program folder ready: $(du -sh "$OUT" | cut -f1)."
