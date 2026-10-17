#!/usr/bin/env bash
# Build SpacePenguin for Android: dist/SpacePenguin-<version>-android-arm64.apk
#
#   ./build-android.sh                  build a debug-signed APK, to install directly
#   ./build-android.sh --release        sign it with your key (see --help)
#   ./build-android.sh --help           all options
#
# The Android app is mobile/: a Qt Quick interface around QtWebView, which shows
# pages with the phone's own Android WebView (QtWebEngine does not exist on
# Android). What it needs is fetched on first use, into the checkout:
#   .qt/<version>/android_arm64_v8a    Qt for Android, with QtWebView (aqtinstall)
#   .qt/<version>/gcc_64               the desktop Qt its tools run from
#   .cache/android-sdk                 the Android SDK, NDK and platform tools
# A JDK 17 or newer must be installed (install-deps.sh does not do it).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT}"

VERSION="$(sed -n 's/^ *VERSION \([0-9][0-9.]*\)$/\1/p' CMakeLists.txt | head -n1)"
QT_VERSION="${QT_VERSION:-6.8.3}"
ABI="arm64-v8a"
QT_ANDROID_ARCH="android_arm64_v8a"
# What Qt 6.8 is built and tested with.
NDK_VERSION="26.1.10909125"
PLATFORM="android-34"
BUILD_TOOLS="34.0.0"
CMDLINE_TOOLS_ZIP="commandlinetools-linux-11076708_latest.zip"
CMDLINE_TOOLS_SHA256="2d2d50857e4eb553af5a6dc3ad507a17adf43d115264b1afc116f95c92e5e258"

BUILD_DIR="${BUILD_DIR:-build-android}"
OUTPUT_DIR="dist"
RELEASE=0

usage() {
    cat <<EOF
Usage: ./build-android.sh [options]

  --release            sign with your own key instead of a debug key; set
                       QT_ANDROID_KEYSTORE_PATH, QT_ANDROID_KEYSTORE_ALIAS,
                       QT_ANDROID_KEYSTORE_STORE_PASS and QT_ANDROID_KEYSTORE_KEY_PASS
  --qt-version <x.y.z> Qt version (default ${QT_VERSION})
  --build-dir <dir>    build directory (default ${BUILD_DIR})
  --output-dir <dir>   where the APK goes (default ${OUTPUT_DIR})
  -h, --help           show this help

Environment: ANDROID_SDK_ROOT to use an existing SDK (it must have NDK ${NDK_VERSION}),
JAVA_HOME for the JDK.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --release) RELEASE=1; shift ;;
        --qt-version) QT_VERSION="$2"; shift 2 ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

step() { printf '\n==> %s\n' "$*"; }
die() { echo "error: $*" >&2; exit 1; }

case "$(uname -s)" in
    Linux) HOST_QT_ARCH=gcc_64; AQT_HOST=linux ;;
    Darwin) HOST_QT_ARCH=macos; AQT_HOST=mac
            CMDLINE_TOOLS_ZIP="commandlinetools-mac-11076708_latest.zip"
            CMDLINE_TOOLS_SHA256="7bc5c72ba0275c80a8f19684fb92793b83a6b5c94d4d179fc5988930282d7e64" ;;
    *) die "build the Android app on Linux or macOS" ;;
esac
for tool in cmake curl unzip java python3; do
    command -v "${tool}" >/dev/null || die "${tool} is missing"
done
java_major="$(java -version 2>&1 | sed -n 's/.*version "\([0-9]*\).*/\1/p' | head -n1)"
[[ "${java_major:-0}" -ge 17 ]] || die "a JDK 17 or newer is needed (found ${java_major:-none})"

sha256() { if command -v sha256sum >/dev/null; then sha256sum "$1"; else shasum -a 256 "$1"; fi | cut -d' ' -f1; }

# ---- Qt: the Android kit and the desktop kit its tools run from ----------------------

QT_DIR="${ROOT}/.qt"
AQT="${QT_DIR}/.venv/bin/aqt"
if [[ ! -x "${AQT}" ]]; then
    step "Installing aqtinstall into .qt/.venv"
    python3 -m venv "${QT_DIR}/.venv"
    "${QT_DIR}/.venv/bin/python" -m pip install --quiet --upgrade pip aqtinstall
fi
QT_HOST_PATH="${QT_DIR}/${QT_VERSION}/${HOST_QT_ARCH}"
QT_ANDROID="${QT_DIR}/${QT_VERSION}/${QT_ANDROID_ARCH}"
if [[ ! -f "${QT_HOST_PATH}/lib/cmake/Qt6WebView/Qt6WebViewConfig.cmake" ]]; then
    step "Installing desktop Qt ${QT_VERSION} (for the build tools)"
    "${AQT}" install-qt "${AQT_HOST}" desktop "${QT_VERSION}" -O "${QT_DIR}" -m qtwebview qtwebengine qtwebchannel qtpositioning
fi
if [[ ! -f "${QT_ANDROID}/lib/cmake/Qt6WebView/Qt6WebViewConfig.cmake" ]]; then
    step "Installing Qt ${QT_VERSION} for Android (${ABI})"
    "${AQT}" install-qt "${AQT_HOST}" android "${QT_VERSION}" "${QT_ANDROID_ARCH}" -O "${QT_DIR}" \
        -m qtwebview qtwebchannel qtpositioning
fi

# ---- Android SDK and NDK ----------------------------------------------------------------

SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-${ROOT}/.cache/android-sdk}}"
SDKMANAGER="${SDK}/cmdline-tools/latest/bin/sdkmanager"
if [[ ! -x "${SDKMANAGER}" ]]; then
    step "Downloading the Android command-line tools into ${SDK}"
    mkdir -p "${SDK}/cmdline-tools"
    zip="${SDK}/${CMDLINE_TOOLS_ZIP}"
    curl -fSL --retry 3 -o "${zip}" "https://dl.google.com/android/repository/${CMDLINE_TOOLS_ZIP}"
    [[ "$(sha256 "${zip}")" == "${CMDLINE_TOOLS_SHA256}" ]] || { rm -f "${zip}"; die "the command-line tools download does not have the expected SHA-256"; }
    rm -rf "${SDK}/cmdline-tools/latest" "${SDK}/cmdline-tools/cmdline-tools"
    unzip -q "${zip}" -d "${SDK}/cmdline-tools"
    mv "${SDK}/cmdline-tools/cmdline-tools" "${SDK}/cmdline-tools/latest"
    rm -f "${zip}"
fi
packages=()
[[ -d "${SDK}/platform-tools" ]] || packages+=("platform-tools")
[[ -d "${SDK}/platforms/${PLATFORM}" ]] || packages+=("platforms;${PLATFORM}")
[[ -d "${SDK}/build-tools/${BUILD_TOOLS}" ]] || packages+=("build-tools;${BUILD_TOOLS}")
[[ -d "${SDK}/ndk/${NDK_VERSION}" ]] || packages+=("ndk;${NDK_VERSION}")
if [[ ${#packages[@]} -gt 0 ]]; then
    step "Installing Android SDK packages: ${packages[*]}"
    yes | "${SDKMANAGER}" --sdk_root="${SDK}" --licenses >/dev/null || true
    "${SDKMANAGER}" --sdk_root="${SDK}" "${packages[@]}"
fi
export ANDROID_SDK_ROOT="${SDK}" ANDROID_HOME="${SDK}"

# ---- Build ------------------------------------------------------------------------------

step "Configuring"
signing=()
if [[ ${RELEASE} -eq 1 ]]; then
    for var in QT_ANDROID_KEYSTORE_PATH QT_ANDROID_KEYSTORE_ALIAS QT_ANDROID_KEYSTORE_STORE_PASS; do
        [[ -n "${!var:-}" ]] || die "--release needs ${var}"
    done
    signing=(-DQT_ANDROID_SIGN_APK=ON)
fi
generator=()
command -v ninja >/dev/null && [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]] && generator=(-G Ninja)
"${QT_ANDROID}/bin/qt-cmake" -S mobile -B "${BUILD_DIR}" ${generator[@]+"${generator[@]}"} \
    -DCMAKE_BUILD_TYPE=Release \
    -DQT_HOST_PATH="${QT_HOST_PATH}" \
    -DANDROID_SDK_ROOT="${SDK}" \
    -DANDROID_NDK_ROOT="${SDK}/ndk/${NDK_VERSION}" \
    -DQT_ANDROID_ABIS="${ABI}" \
    ${signing[@]+"${signing[@]}"}

step "Building the APK"
cmake --build "${BUILD_DIR}" --parallel "$("${ROOT}/tools/build-jobs.sh")" --target apk

apk="$(find "${BUILD_DIR}/android-build" -name '*.apk' -path '*release*' | head -n1)"
[[ -n "${apk}" ]] || apk="$(find "${BUILD_DIR}/android-build" -name '*.apk' | head -n1)"
[[ -n "${apk}" ]] || die "no APK was produced"
mkdir -p "${OUTPUT_DIR}"
OUTPUT="${OUTPUT_DIR}/SpacePenguin-${VERSION}-android-arm64.apk"
cp "${apk}" "${OUTPUT}"
(cd "${OUTPUT_DIR}" && sha256sum "$(basename "${OUTPUT}")" > "$(basename "${OUTPUT}").sha256")

step "Created ${OUTPUT} ($(du -h "${OUTPUT}" | cut -f1))"
echo "    Install on a phone with USB debugging on: adb install -r ${OUTPUT}"
[[ ${RELEASE} -eq 1 ]] || echo "    Signed with a debug key: fine for installing directly, not for Google Play."
