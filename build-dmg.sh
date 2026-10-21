#!/usr/bin/env bash
# Build SpacePenguin.app and package it as a disk image and a zip:
#
#   ./build-dmg.sh                       dist/SpacePenguin-<version>-macos.dmg and .zip
#   ./build-dmg.sh --help                all options
#
# `cmake --install` runs macdeployqt, which copies the Qt frameworks and
# QtWebEngine's helper app into the bundle. The bundle is then signed from the
# inside out: ad-hoc by default, or with a Developer ID (--sign), in which case it
# uses the hardened runtime plus the JIT entitlements QtWebEngine needs, and can be
# notarised (--notarize) so Gatekeeper opens it without warnings.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT}"

VERSION="$(sed -n 's/^set(SPACEPENGUIN_VERSION_NAME "\(.*\)")$/\1/p' CMakeLists.txt | head -n1)"
BUILD_DIR="${BUILD_DIR:-build-macos}"
OUTPUT_DIR="dist"
QT_PREFIX=""
ARCHS=""
IDENTITY="${CODESIGN_IDENTITY:-}"
NOTARIZE=0
SKIP_BUILD=0
MAKE_DMG=1
SMOKE_TEST=1
ENTITLEMENTS="${ROOT}/packaging/macos/entitlements.plist"

usage() {
    cat <<EOF
Usage: ./build-dmg.sh [options] [version]

  --version <name>      version in the file names (default ${VERSION})
  --qt <prefix>         Qt to build against (default: .qt/, CMAKE_PREFIX_PATH, Homebrew)
  --universal           build for both arm64 and x86_64 (Qt from install-deps is universal)
  --arch <arm64|x86_64> build for one architecture (default: this Mac's)
  --sign <identity>     sign with this Developer ID ("Developer ID Application: …");
                        also read from CODESIGN_IDENTITY. Default: ad-hoc
  --notarize            notarise and staple the DMG (needs --sign). Credentials come
                        from NOTARY_PROFILE (a notarytool keychain profile) or from
                        APPLE_ID, APPLE_TEAM_ID and APPLE_APP_PASSWORD
  --app-only            make the .app and its zip, but no DMG
  --build-dir <dir>     build directory (default ${BUILD_DIR}; env BUILD_DIR)
  --output-dir <dir>    where the packages go (default ${OUTPUT_DIR})
  --skip-build          reuse the existing build directory as it is
  --no-smoke-test       do not launch the finished app to check it
  -h, --help            show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version) VERSION="$2"; shift 2 ;;
        --qt) QT_PREFIX="$2"; shift 2 ;;
        --universal) ARCHS="arm64;x86_64"; shift ;;
        --arch) ARCHS="$2"; shift 2 ;;
        --sign) IDENTITY="$2"; shift 2 ;;
        --notarize) NOTARIZE=1; shift ;;
        --app-only) MAKE_DMG=0; shift ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --skip-build) SKIP_BUILD=1; shift ;;
        --no-smoke-test) SMOKE_TEST=0; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
        *) VERSION="$1"; shift ;;
    esac
done

step() { printf '\n==> %s\n' "$*"; }
die() { echo "error: $*" >&2; exit 1; }

[[ "$(uname -s)" == Darwin ]] || die "the macOS app and DMG are built on macOS"
[[ ${NOTARIZE} -eq 0 || -n "${IDENTITY}" ]] || die "--notarize needs a Developer ID (--sign)"
command -v cmake >/dev/null || die "cmake is missing; run ./install-deps.sh"

# ---- Find Qt ----------------------------------------------------------------------

if [[ -z "${QT_PREFIX}" ]]; then
    for config in .qt/*/*/lib/cmake/Qt6WebEngineWidgets/Qt6WebEngineWidgetsConfig.cmake; do
        [[ -f "${config}" ]] && QT_PREFIX="$(cd "$(dirname "${config}")/../../.." && pwd)"
    done
fi
[[ -z "${QT_PREFIX}" && -n "${CMAKE_PREFIX_PATH:-}" ]] && QT_PREFIX="${CMAKE_PREFIX_PATH%%:*}"
if [[ -z "${QT_PREFIX}" ]] && command -v brew >/dev/null && brew --prefix qt >/dev/null 2>&1; then
    QT_PREFIX="$(brew --prefix qt)"
fi
[[ -n "${QT_PREFIX}" && -d "${QT_PREFIX}" ]] ||
    die "no Qt found; run ./install-deps.sh or pass --qt /path/to/Qt/6.8.3/macos"

DIST="${ROOT}/${BUILD_DIR}/dist"          # Qt's deploy step wants an absolute prefix
APP="${DIST}/SpacePenguin.app"
mkdir -p "${OUTPUT_DIR}"
OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"
BASENAME="SpacePenguin-${VERSION}-macos"

echo "SpacePenguin ${VERSION}, Qt from ${QT_PREFIX}, architectures: ${ARCHS:-$(uname -m)}"

# ---- Build and deploy ----------------------------------------------------------------

if [[ ${SKIP_BUILD} -eq 0 ]]; then
    step "Building"
    generator=()
    command -v ninja >/dev/null && [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]] && generator=(-G Ninja)
    cmake -S . -B "${BUILD_DIR}" ${generator[@]+"${generator[@]}"} \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="${QT_PREFIX}" \
        -DSPACEPENGUIN_BUILD_TESTS=OFF \
        ${ARCHS:+-DCMAKE_OSX_ARCHITECTURES="${ARCHS}"}
    cmake --build "${BUILD_DIR}" --parallel "$("${ROOT}/tools/build-jobs.sh")"
fi

step "Installing and deploying Qt into the bundle (macdeployqt)"
rm -rf "${DIST}"
cmake --install "${BUILD_DIR}" --prefix "${DIST}"
[[ -d "${APP}" ]] || die "the install step did not produce ${APP}"
[[ -d "${APP}/Contents/Frameworks/QtWebEngineCore.framework" ]] ||
    die "QtWebEngineCore.framework is missing from the bundle; macdeployqt did not run"

# ---- Sign ------------------------------------------------------------------------------

# sign <path> [executable]: executables (the app and QtWebEngine's helper app) get
# the entitlements; libraries and frameworks are signed without them.
sign() {
    local target="$1" entitlements=()
    [[ "${2:-}" == executable ]] && entitlements=(--entitlements "${ENTITLEMENTS}")
    if [[ -n "${IDENTITY}" ]]; then
        codesign --force --timestamp --options runtime ${entitlements[@]+"${entitlements[@]}"} \
            --sign "${IDENTITY}" "${target}"
    else
        codesign --force ${entitlements[@]+"${entitlements[@]}"} --sign - "${target}"
    fi
}

step "Signing ($([[ -n "${IDENTITY}" ]] && echo "${IDENTITY}" || echo "ad-hoc"))"
# Inside out: loose libraries and plugins, nested helper apps, frameworks, then the
# app itself. QtWebEngineProcess is a nested app and needs the JIT entitlements too.
while IFS= read -r -d '' file; do sign "${file}"; done < <(
    find "${APP}/Contents" -type f \( -name '*.dylib' -o -name '*.so' \) -print0)
while IFS= read -r -d '' helper; do sign "${helper}" executable; done < <(
    find "${APP}/Contents/Frameworks" -type d -name '*.app' -print0)
while IFS= read -r -d '' framework; do sign "${framework}"; done < <(
    find "${APP}/Contents/Frameworks" -maxdepth 1 -type d -name '*.framework' -print0)
sign "${APP}" executable
codesign --verify --deep --strict --verbose=1 "${APP}"

# ---- Check -----------------------------------------------------------------------------

if [[ ${SMOKE_TEST} -eq 1 ]]; then
    step "Smoke test: launching the app"
    # The bundle carries the cocoa platform plugin only, which is what users get;
    # --version exits before any window opens.
    reported="$(env -i HOME="$(mktemp -d)" PATH=/usr/bin:/bin \
        "${APP}/Contents/MacOS/SpacePenguin" --version 2>&1 | tail -n1)"
    echo "    ${reported}"
    [[ "${reported}" == *"SpacePenguin ${VERSION}"* ]] ||
        die "the app did not start correctly (expected \"SpacePenguin ${VERSION}\")"
fi

# ---- Package ---------------------------------------------------------------------------

step "Zipping the app"
rm -f "${OUTPUT_DIR}/${BASENAME}.zip"
ditto -c -k --sequesterRsrc --keepParent "${APP}" "${OUTPUT_DIR}/${BASENAME}.zip"
PACKAGES=("${BASENAME}.zip")

if [[ ${MAKE_DMG} -eq 1 ]]; then
    step "Creating the disk image"
    DMG="${OUTPUT_DIR}/${BASENAME}.dmg"
    STAGING="$(mktemp -d)"
    trap 'rm -rf "${STAGING}"' EXIT
    ditto "${APP}" "${STAGING}/SpacePenguin.app"
    ln -s /Applications "${STAGING}/Applications"
    rm -f "${DMG}"
    hdiutil create -volname "SpacePenguin ${VERSION}" -srcfolder "${STAGING}" \
        -fs HFS+ -format UDZO -imagekey zlib-level=9 -ov "${DMG}" >/dev/null
    [[ -n "${IDENTITY}" ]] && codesign --force --timestamp --sign "${IDENTITY}" "${DMG}"
    hdiutil verify "${DMG}" >/dev/null
    PACKAGES+=("${BASENAME}.dmg")

    if [[ ${NOTARIZE} -eq 1 ]]; then
        step "Notarising"
        if [[ -n "${NOTARY_PROFILE:-}" ]]; then
            xcrun notarytool submit "${DMG}" --keychain-profile "${NOTARY_PROFILE}" --wait
        else
            : "${APPLE_ID:?set NOTARY_PROFILE, or APPLE_ID, APPLE_TEAM_ID and APPLE_APP_PASSWORD}"
            : "${APPLE_TEAM_ID:?}" "${APPLE_APP_PASSWORD:?}"
            xcrun notarytool submit "${DMG}" --apple-id "${APPLE_ID}" --team-id "${APPLE_TEAM_ID}" \
                --password "${APPLE_APP_PASSWORD}" --wait
        fi
        xcrun stapler staple "${DMG}"
        spctl --assess --type open --context context:primary-signature --verbose "${DMG}"
    fi
fi

(cd "${OUTPUT_DIR}" && shasum -a 256 "${PACKAGES[@]}" > "${BASENAME}.sha256")

step "Created"
for package in "${PACKAGES[@]}"; do
    echo "    ${OUTPUT_DIR}/${package} ($(du -h "${OUTPUT_DIR}/${package}" | cut -f1))"
done
if [[ -z "${IDENTITY}" ]]; then
    echo "    Ad-hoc signed: on another Mac, right-click the app and choose Open the first time."
fi
