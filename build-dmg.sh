#!/usr/bin/env bash
# Build a macOS disk image: ./build-dmg.sh [version]
#
# `cmake --install` runs macdeployqt, so the installed SpacePenguin.app already
# carries Qt, QtWebEngineCore and its helper process. Set CODESIGN_IDENTITY to sign
# with a Developer ID; without it the app is ad-hoc signed, which runs on the
# machine that built it and needs right-click > Open elsewhere.
set -euo pipefail

VERSION="${1:-0.1.0}"
BUILD_DIR="${BUILD_DIR:-build-macos}"
DIST_DIR="$(pwd)/${BUILD_DIR}/dist"  # Qt's deploy step wants an absolute prefix
APP="${DIST_DIR}/SpacePenguin.app"
DMG="SpacePenguin-${VERSION}-macos.dmg"

if [[ "$(uname)" != "Darwin" ]]; then
    echo "DMGs are built on macOS." >&2
    exit 1
fi

echo "==> Building SpacePenguin ${VERSION}"
cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -DSPACEPENGUIN_BUILD_TESTS=OFF \
    ${CMAKE_OSX_ARCHITECTURES:+-DCMAKE_OSX_ARCHITECTURES="${CMAKE_OSX_ARCHITECTURES}"}
cmake --build "${BUILD_DIR}" --parallel

rm -rf "${DIST_DIR}"
cmake --install "${BUILD_DIR}" --prefix "${DIST_DIR}"

if [[ ! -d "${APP}" ]]; then
    echo "Install did not produce ${APP}" >&2
    exit 1
fi

echo "==> Signing"
if [[ -n "${CODESIGN_IDENTITY:-}" ]]; then
    # Hardened runtime, as notarisation requires, with the JIT entitlements
    # QtWebEngine needs to run JavaScript.
    codesign --force --deep --timestamp --options runtime \
        --entitlements packaging/macos/entitlements.plist \
        --sign "${CODESIGN_IDENTITY}" "${APP}"
else
    codesign --force --deep --sign - "${APP}"
fi

echo "==> Creating ${DMG}"
STAGING="$(mktemp -d)"
trap 'rm -rf "${STAGING}"' EXIT
cp -R "${APP}" "${STAGING}/"
ln -s /Applications "${STAGING}/Applications"
rm -f "${DMG}"
hdiutil create -volname "SpacePenguin ${VERSION}" -srcfolder "${STAGING}" \
    -ov -format UDZO "${DMG}"

if [[ -n "${CODESIGN_IDENTITY:-}" ]]; then
    codesign --force --sign "${CODESIGN_IDENTITY}" "${DMG}"
    # Notarise with: xcrun notarytool submit "${DMG}" --keychain-profile <profile> --wait
    #                xcrun stapler staple "${DMG}"
fi

echo "==> Created ${DMG} ($(du -h "${DMG}" | cut -f1))"
