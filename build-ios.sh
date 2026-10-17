#!/usr/bin/env bash
# Build SpacePenguin for iPhone and iPad (macOS with Xcode only).
#
#   ./build-ios.sh                      an unsigned app for the iOS Simulator:
#                                       dist/SpacePenguin-<version>-ios-simulator.zip
#   ./build-ios.sh --team ABCDE12345    a signed .ipa for real devices, with your
#                                       Apple Developer team (Xcode must be signed in)
#   ./build-ios.sh --help               all options
#
# The iOS app is mobile/: a Qt Quick interface around QtWebView, which shows pages
# with WKWebView, the only engine iOS allows. Qt for iOS and the macOS Qt its tools
# run from are fetched into .qt/ on first use.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT}"

VERSION="$(sed -n 's/^ *VERSION \([0-9][0-9.]*\)$/\1/p' CMakeLists.txt | head -n1)"
QT_VERSION="${QT_VERSION:-6.8.3}"
BUILD_DIR="${BUILD_DIR:-build-ios}"
OUTPUT_DIR="dist"
TEAM=""
EXPORT_METHOD="development"

usage() {
    cat <<EOF
Usage: ./build-ios.sh [options]

  --team <id>            sign for devices with this Apple Developer team ID
  --export-method <m>    with --team: development (default), ad-hoc or app-store
  --qt-version <x.y.z>   Qt version (default ${QT_VERSION})
  --build-dir <dir>      build directory (default ${BUILD_DIR})
  --output-dir <dir>     where the result goes (default ${OUTPUT_DIR})
  -h, --help             show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --team) TEAM="$2"; shift 2 ;;
        --export-method) EXPORT_METHOD="$2"; shift 2 ;;
        --qt-version) QT_VERSION="$2"; shift 2 ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

step() { printf '\n==> %s\n' "$*"; }
die() { echo "error: $*" >&2; exit 1; }

[[ "$(uname -s)" == Darwin ]] || die "iOS apps are built on macOS with Xcode"
command -v xcodebuild >/dev/null || die "Xcode is missing (install it from the App Store, then run xcode-select --install)"
for tool in cmake python3 ditto; do
    command -v "${tool}" >/dev/null || die "${tool} is missing"
done
case "${EXPORT_METHOD}" in development|ad-hoc|app-store) ;; *) die "unknown export method ${EXPORT_METHOD}" ;; esac

# ---- Qt for iOS, and the macOS Qt its tools run from -------------------------------------

QT_DIR="${ROOT}/.qt"
AQT="${QT_DIR}/.venv/bin/aqt"
if [[ ! -x "${AQT}" ]]; then
    step "Installing aqtinstall into .qt/.venv"
    python3 -m venv "${QT_DIR}/.venv"
    "${QT_DIR}/.venv/bin/python" -m pip install --quiet --upgrade pip aqtinstall
fi
QT_HOST_PATH="${QT_DIR}/${QT_VERSION}/macos"
QT_IOS="${QT_DIR}/${QT_VERSION}/ios"
if [[ ! -f "${QT_HOST_PATH}/lib/cmake/Qt6WebView/Qt6WebViewConfig.cmake" ]]; then
    step "Installing macOS Qt ${QT_VERSION} (for the build tools)"
    "${AQT}" install-qt mac desktop "${QT_VERSION}" clang_64 -O "${QT_DIR}" -m qtwebview qtwebengine qtwebchannel qtpositioning
fi
if [[ ! -f "${QT_IOS}/lib/cmake/Qt6WebView/Qt6WebViewConfig.cmake" ]]; then
    step "Installing Qt ${QT_VERSION} for iOS"
    "${AQT}" install-qt mac ios "${QT_VERSION}" ios -O "${QT_DIR}" -m qtwebview
fi

# ---- Build ------------------------------------------------------------------------------

step "Configuring (Xcode project)"
signing=()
[[ -n "${TEAM}" ]] && signing=(-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="${TEAM}")
"${QT_IOS}/bin/qt-cmake" -S mobile -B "${BUILD_DIR}" -G Xcode \
    -DQT_HOST_PATH="${QT_HOST_PATH}" \
    -DCMAKE_XCODE_GENERATE_SCHEME=ON \
    ${signing[@]+"${signing[@]}"}

mkdir -p "${OUTPUT_DIR}"
if [[ -z "${TEAM}" ]]; then
    step "Building for the iOS Simulator"
    xcodebuild -project "${BUILD_DIR}/SpacePenguinMobile.xcodeproj" -scheme spacepenguin_mobile \
        -configuration Release -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' \
        -derivedDataPath "${BUILD_DIR}/derived" CODE_SIGNING_ALLOWED=NO build
    app="$(find "${BUILD_DIR}/derived/Build/Products" -name 'SpacePenguin.app' -path '*iphonesimulator*' | head -n1)"
    [[ -n "${app}" ]] || die "no app was produced"
    OUTPUT="${OUTPUT_DIR}/SpacePenguin-${VERSION}-ios-simulator.zip"
    rm -f "${OUTPUT}"
    ditto -c -k --keepParent "${app}" "${OUTPUT}"
    step "Created ${OUTPUT}"
    echo "    Try it: unzip it, then xcrun simctl install booted SpacePenguin.app"
else
    step "Archiving for devices (team ${TEAM})"
    archive="${BUILD_DIR}/SpacePenguin.xcarchive"
    xcodebuild -project "${BUILD_DIR}/SpacePenguinMobile.xcodeproj" -scheme spacepenguin_mobile \
        -configuration Release -destination 'generic/platform=iOS' -archivePath "${archive}" \
        -allowProvisioningUpdates archive
    options="${BUILD_DIR}/ExportOptions.plist"
    cat > "${options}" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>method</key><string>${EXPORT_METHOD}</string>
  <key>teamID</key><string>${TEAM}</string>
  <key>signingStyle</key><string>automatic</string>
</dict></plist>
EOF
    xcodebuild -exportArchive -archivePath "${archive}" -exportOptionsPlist "${options}" \
        -exportPath "${BUILD_DIR}/export" -allowProvisioningUpdates
    ipa="$(find "${BUILD_DIR}/export" -name '*.ipa' | head -n1)"
    [[ -n "${ipa}" ]] || die "no .ipa was produced"
    OUTPUT="${OUTPUT_DIR}/SpacePenguin-${VERSION}-ios.ipa"
    cp "${ipa}" "${OUTPUT}"
    step "Created ${OUTPUT}"
fi
(cd "${OUTPUT_DIR}" && shasum -a 256 "$(basename "${OUTPUT}")" > "$(basename "${OUTPUT}").sha256")
