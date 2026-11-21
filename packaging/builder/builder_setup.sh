#!/usr/bin/env bash
#
# SPDX-License-Identifier: AGPL-3.0-only
#
# Chrono Vault, a password manager.
# Copyright (C) 2026 Rob Vandenberg
#
# --- Version ------------------------------------------------------------
# builder_setup.sh 0.0.4
#
# --- Version history ----------------------------------------------------
# v0.0.4: Initial release, modelled on Chrono Viewer's
#
# Runs once, while the builder image is made (packaging/builder/Containerfile). It puts into Ubuntu 22.04 what a
# release build needs and the system does not have:
#   1. a g++ new enough for C++20, Ninja, patchelf and ccache, from Ubuntu itself;
#   2. a Python virtual environment holding aqtinstall and a recent CMake;
#   3. Qt, installed by aqtinstall; the system's own Qt is never used;
#   4. OpenSSL 3.5, built here from its own source: Ubuntu 22.04 carries OpenSSL 3.0, which has no Argon2id (D-013).
# The source archive is checked against the SHA-256 written below, measured when it was first downloaded for this
# recipe on 21 November 2026, and the same as the one OpenSSL publishes.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

QT_VERSION=6.10.2
OPENSSL_VERSION=3.5.9
OPENSSL_SHA256=603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a

# 1. From Ubuntu itself, with the system libraries Qt needs to run here.
apt-get update
apt-get install -y --no-install-recommends \
    ca-certificates curl xz-utils file perl \
    g++-12 make ninja-build patchelf ccache pkg-config python3 python3-venv \
    libgl-dev libegl-dev libopengl-dev libxkbcommon-dev \
    libfontconfig1 libfreetype6 libdbus-1-3 libglib2.0-0 libgssapi-krb5-2 \
    libx11-6 libx11-xcb1 libxkbcommon-x11-0 libxcb1 libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
    libxcb-randr0 libxcb-render0 libxcb-render-util0 libxcb-shape0 libxcb-shm0 libxcb-sync1 libxcb-xfixes0 \
    libxcb-xinput0 libxcb-xkb1 libxcb-util1 libwayland-client0 libwayland-cursor0 libwayland-egl1
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100
update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100
update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-12 100
update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-12 100
rm -rf /var/lib/apt/lists/*

# 2. The tools, in a virtual environment of their own, at fixed versions.
python3 -m venv /opt/tools
/opt/tools/bin/pip install --quiet --no-cache-dir aqtinstall==3.3.0 cmake==3.31.6
export PATH="/opt/tools/bin:$PATH"

# 3. Qt; its base brings Qt's own SVG, QML and Quick, the translation tools and Wayland.
aqt install-qt linux desktop "$QT_VERSION" linux_gcc_64 --outputdir /opt/qt
QT="/opt/qt/$QT_VERSION/gcc_64"
for library in Core Gui Widgets Svg Qml Quick QuickControls2; do
    test -f "$QT/lib/libQt6$library.so.6" || { echo "Qt's $library is missing from the install." >&2; exit 1; }
done

# 4. OpenSSL: only its crypto library is used; built shared, into /opt/openssl.
mkdir -p /setup/sources
cd /setup/sources
curl -sSfL --retry 3 -o openssl.tar.gz \
    "https://github.com/openssl/openssl/releases/download/openssl-$OPENSSL_VERSION/openssl-$OPENSSL_VERSION.tar.gz"
echo "$OPENSSL_SHA256  openssl.tar.gz" | sha256sum --check --quiet
mkdir openssl
tar -xf openssl.tar.gz -C openssl --strip-components=1
cd openssl
./Configure linux-x86_64 --prefix=/opt/openssl --libdir=lib shared no-tests no-docs >/dev/null
make -j"$(nproc)" >/dev/null
make install_sw >/dev/null
mkdir -p /opt/openssl/licences
cp LICENSE.txt /opt/openssl/licences/
cd /
rm -rf /setup/sources
test -f /opt/openssl/lib/libcrypto.so.3
echo "Builder image ready: Qt $QT_VERSION in $QT, OpenSSL $OPENSSL_VERSION in /opt/openssl."
