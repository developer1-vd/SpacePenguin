#!/usr/bin/env bash
# Build a Linux AppImage: ./build-appimage.sh [version]
#
# Uses linuxdeploy with its Qt plugin, which knows how to bundle QtWebEngine
# (QtWebEngineProcess, resources, locales). Both are downloaded into
# .cache/linuxdeploy on first use. Point CMAKE_PREFIX_PATH (or QMAKE) at your Qt
# if it is not the system one.
set -euo pipefail

VERSION="${1:-0.1.0}"
ARCH="$(uname -m)"
BUILD_DIR="${BUILD_DIR:-build-appimage}"
APPDIR="${BUILD_DIR}/AppDir"
TOOLS_DIR=".cache/linuxdeploy"
APP_ID="org.spacepenguin.SpacePenguin"

if [[ "$(uname)" != "Linux" ]]; then
    echo "AppImages are built on Linux." >&2
    exit 1
fi

echo "==> Building SpacePenguin ${VERSION} (${ARCH})"
cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DSPACEPENGUIN_BUILD_TESTS=OFF -DSPACEPENGUIN_INSTALL_QT_CONF=OFF
cmake --build "${BUILD_DIR}" --parallel

rm -rf "${APPDIR}"
DESTDIR="$(pwd)/${APPDIR}" cmake --install "${BUILD_DIR}"

mkdir -p "${TOOLS_DIR}"
fetch() {
    local name="$1" url="$2"
    if [[ ! -x "${TOOLS_DIR}/${name}" ]]; then
        echo "==> Downloading ${name}"
        curl -fsSL -o "${TOOLS_DIR}/${name}" "${url}"
        chmod +x "${TOOLS_DIR}/${name}"
    fi
}
fetch "linuxdeploy-${ARCH}.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
fetch "linuxdeploy-plugin-qt-${ARCH}.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ARCH}.AppImage"

# The Qt plugin locates Qt through qmake.
if [[ -z "${QMAKE:-}" ]]; then
    QMAKE="$(command -v qmake6 || command -v qmake || true)"
    if [[ -n "${CMAKE_PREFIX_PATH:-}" && -x "${CMAKE_PREFIX_PATH%%:*}/bin/qmake" ]]; then
        QMAKE="${CMAKE_PREFIX_PATH%%:*}/bin/qmake"
    fi
fi
if [[ -z "${QMAKE}" ]]; then
    echo "qmake not found; set QMAKE=/path/to/Qt/bin/qmake" >&2
    exit 1
fi
export QMAKE
# linuxdeploy resolves the binary's libraries through the normal search path.
QT_LIBS="$("${QMAKE}" -query QT_INSTALL_LIBS)"
export LD_LIBRARY_PATH="${QT_LIBS}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export EXTRA_QT_MODULES="webenginecore;webenginewidgets"
export LDAI_OUTPUT="SpacePenguin-${VERSION}-${ARCH}.AppImage"
# Lets the AppImages run inside containers without FUSE.
export APPIMAGE_EXTRACT_AND_RUN=1

echo "==> Bundling with linuxdeploy"
PATH="$(pwd)/${TOOLS_DIR}:${PATH}" "${TOOLS_DIR}/linuxdeploy-${ARCH}.AppImage" \
    --appdir "${APPDIR}" \
    --desktop-file "${APPDIR}/usr/share/applications/${APP_ID}.desktop" \
    --icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${APP_ID}.png" \
    --plugin qt \
    --output appimage

echo "==> Created ${LDAI_OUTPUT}"
