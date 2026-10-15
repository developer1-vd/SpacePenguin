#!/usr/bin/env bash
# Build a Linux AppImage: one file that runs on any distribution whose glibc is at
# least as new as the build machine's. CI builds on Ubuntu 22.04 (glibc 2.35), so
# its AppImages run on Ubuntu 22.04, Debian 12, Fedora 36, RHEL 9, openSUSE Leap
# 15.6 and everything newer.
#
#   ./build-appimage.sh                    build dist/SpacePenguin-<version>-<arch>.AppImage
#   ./build-appimage.sh --help             all options
#
# Qt is taken from .qt/ (./install-deps.sh), $CMAKE_PREFIX_PATH or $QMAKE, in that
# order of preference after an explicit --qt. linuxdeploy and its Qt plugin, which
# know how to bundle QtWebEngine, are downloaded into .cache/ on first use.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT}"

APP_ID="org.spacepenguin.SpacePenguin"
VERSION="$(sed -n 's/^ *VERSION \([0-9][0-9.]*\)$/\1/p' CMakeLists.txt | head -n1)"
ARCH="$(uname -m)"
BUILD_DIR="${BUILD_DIR:-build-appimage}"
OUTPUT_DIR="dist"
QT_PREFIX=""
SKIP_BUILD=0
UPDATE_INFO=""
SIGN=0
SMOKE_TEST=1

usage() {
    cat <<EOF
Usage: ./build-appimage.sh [options] [version]

  --version <x.y.z>     version in the file name (default ${VERSION}, from CMakeLists.txt)
  --qt <prefix>         Qt installation to bundle (default: .qt/, CMAKE_PREFIX_PATH, QMAKE)
  --build-dir <dir>     build directory (default ${BUILD_DIR}; env BUILD_DIR)
  --output-dir <dir>    where the AppImage goes (default ${OUTPUT_DIR})
  --skip-build          reuse the existing build directory as it is
  --update-info <info>  embed AppImageUpdate information, for example
                        "gh-releases-zsync|developer1-vd|SpacePenguin|latest|SpacePenguin-*-${ARCH}.AppImage.zsync"
  --sign                sign the AppImage with your default GPG key
  --no-smoke-test       do not launch the finished AppImage to check it
  -h, --help            show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version) VERSION="$2"; shift 2 ;;
        --qt) QT_PREFIX="$2"; shift 2 ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --skip-build) SKIP_BUILD=1; shift ;;
        --update-info) UPDATE_INFO="$2"; shift 2 ;;
        --sign) SIGN=1; shift ;;
        --no-smoke-test) SMOKE_TEST=0; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
        *) VERSION="$1"; shift ;;   # positional version, as in earlier releases
    esac
done

step() { printf '\n==> %s\n' "$*"; }
die() { echo "error: $*" >&2; exit 1; }

[[ "$(uname -s)" == Linux ]] || die "AppImages are built on Linux"
case "${ARCH}" in x86_64|aarch64) ;; *) die "linuxdeploy has no build for ${ARCH}" ;; esac
for tool in cmake curl file; do
    command -v "${tool}" >/dev/null || die "${tool} is missing; run ./install-deps.sh"
done

# ---- Find Qt ----------------------------------------------------------------------

if [[ -z "${QT_PREFIX}" ]]; then
    for config in .qt/*/*/lib/cmake/Qt6WebEngineWidgets/Qt6WebEngineWidgetsConfig.cmake; do
        [[ -f "${config}" ]] && QT_PREFIX="$(cd "$(dirname "${config}")/../../.." && pwd)"
    done
fi
if [[ -z "${QT_PREFIX}" && -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    QT_PREFIX="${CMAKE_PREFIX_PATH%%:*}"
fi
if [[ -z "${QT_PREFIX}" && -n "${QMAKE:-}" ]]; then
    QT_PREFIX="$("${QMAKE}" -query QT_INSTALL_PREFIX)"
fi
if [[ -z "${QT_PREFIX}" ]]; then
    QTPATHS="$(command -v qtpaths6 || command -v qtpaths || true)"
    [[ -n "${QTPATHS}" ]] && QT_PREFIX="$("${QTPATHS}" --qt-query QT_INSTALL_PREFIX)"
fi
[[ -n "${QT_PREFIX}" && -d "${QT_PREFIX}" ]] ||
    die "no Qt found; run ./install-deps.sh or pass --qt /path/to/Qt/6.8.3/gcc_64"

QMAKE="${QT_PREFIX}/bin/qmake"
[[ -x "${QMAKE}" ]] || QMAKE="${QT_PREFIX}/bin/qmake6"
[[ -x "${QMAKE}" ]] || QMAKE="$(command -v qmake6 || command -v qmake || true)"
[[ -x "${QMAKE}" ]] || die "qmake not found next to ${QT_PREFIX}; linuxdeploy's Qt plugin needs it"
QT_VERSION="$("${QMAKE}" -query QT_VERSION)"
[[ "$(printf '%s\n6.8.0\n' "${QT_VERSION}" | sort -V | head -n1)" == "6.8.0" ]] ||
    die "Qt ${QT_VERSION} is too old; SpacePenguin needs 6.8 or newer"

echo "SpacePenguin ${VERSION} for ${ARCH}, Qt ${QT_VERSION} from ${QT_PREFIX}"

# ---- Build and install into the AppDir ---------------------------------------------

APPDIR="${BUILD_DIR}/AppDir"
if [[ ${SKIP_BUILD} -eq 0 ]]; then
    step "Building"
    generator=()
    command -v ninja >/dev/null && [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]] && generator=(-G Ninja)
    cmake -S . -B "${BUILD_DIR}" ${generator[@]+"${generator[@]}"} \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="${QT_PREFIX}" \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DSPACEPENGUIN_BUILD_TESTS=OFF \
        -DSPACEPENGUIN_INSTALL_QT_CONF=OFF \
        -DSPACEPENGUIN_INSTALL_APPARMOR_PROFILE=OFF
    cmake --build "${BUILD_DIR}" --parallel "$("${ROOT}/tools/build-jobs.sh")"
fi

step "Installing into ${APPDIR}"
rm -rf "${APPDIR}"
DESTDIR="${ROOT}/${APPDIR}" cmake --install "${BUILD_DIR}"

# ---- Tools ---------------------------------------------------------------------------

TOOLS_DIR="${ROOT}/.cache/linuxdeploy"
mkdir -p "${TOOLS_DIR}"
fetch() {
    local name="$1" url="$2"
    if [[ ! -x "${TOOLS_DIR}/${name}" ]]; then
        step "Downloading ${name}"
        curl -fSL --retry 3 -o "${TOOLS_DIR}/${name}.part" "${url}"
        chmod +x "${TOOLS_DIR}/${name}.part"
        mv "${TOOLS_DIR}/${name}.part" "${TOOLS_DIR}/${name}"
    fi
}
fetch "linuxdeploy-${ARCH}.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
fetch "linuxdeploy-plugin-qt-${ARCH}.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ARCH}.AppImage"

# ---- Bundle --------------------------------------------------------------------------

mkdir -p "${OUTPUT_DIR}"
OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"   # absolute, whether given relative or not
OUTPUT="${OUTPUT_DIR}/SpacePenguin-${VERSION}-${ARCH}.AppImage"
rm -f "${OUTPUT}" "${OUTPUT}.zsync"

QT_LIBS="$("${QMAKE}" -query QT_INSTALL_LIBS)"
export QMAKE
export LD_LIBRARY_PATH="${QT_LIBS}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export EXTRA_QT_MODULES="webenginecore;webenginewidgets"
# xcb is always bundled; add native Wayland, and offscreen for headless use, when
# this Qt has them. A distribution's Qt keeps Wayland in a separate package
# (qt6-wayland on Debian and Ubuntu), and linuxdeploy fails on a missing plugin.
QT_PLUGINS="$("${QMAKE}" -query QT_INSTALL_PLUGINS)"
extra_plugins=()
for plugin in libqwayland-egl.so libqwayland-generic.so libqoffscreen.so; do
    if [[ -f "${QT_PLUGINS}/platforms/${plugin}" ]]; then
        extra_plugins+=("${plugin}")
    else
        echo "warning: ${QT_PLUGINS}/platforms/${plugin} is missing; the AppImage will lack it" >&2
    fi
done
if [[ ! -f "${QT_PLUGINS}/platforms/libqwayland-egl.so" ]]; then
    echo "         (it then runs through XWayland on Wayland desktops; install qt6-wayland to bundle native Wayland)" >&2
fi
EXTRA_PLATFORM_PLUGINS="$(IFS=';'; echo "${extra_plugins[*]}")"
export EXTRA_PLATFORM_PLUGINS
export LDAI_OUTPUT="${OUTPUT}"
export APPIMAGE_EXTRACT_AND_RUN=1           # works in containers without FUSE
export NO_STRIP=1                           # Qt's libraries are already stripped
[[ -n "${UPDATE_INFO}" ]] && export LDAI_UPDATE_INFORMATION="${UPDATE_INFO}"
[[ ${SIGN} -eq 1 ]] && export LDAI_SIGN=1

step "Bundling Qt and QtWebEngine with linuxdeploy"
PATH="${TOOLS_DIR}:${PATH}" "${TOOLS_DIR}/linuxdeploy-${ARCH}.AppImage" \
    --appdir "${APPDIR}" \
    --desktop-file "${APPDIR}/usr/share/applications/${APP_ID}.desktop" \
    --icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${APP_ID}.png" \
    --plugin qt \
    --output appimage

[[ -f "${OUTPUT}" ]] || die "linuxdeploy did not produce ${OUTPUT}"
# linuxdeploy writes the .zsync next to the current directory.
[[ -f "$(basename "${OUTPUT}").zsync" ]] && mv "$(basename "${OUTPUT}").zsync" "${OUTPUT_DIR}/"

# ---- Check ---------------------------------------------------------------------------

if [[ ${SMOKE_TEST} -eq 1 && ! -f "${QT_PLUGINS}/platforms/libqoffscreen.so" ]]; then
    echo "warning: skipping the launch check, which needs Qt's offscreen platform plugin" >&2
elif [[ ${SMOKE_TEST} -eq 1 ]]; then
    step "Smoke test: launching the AppImage"
    # A clean environment, so nothing outside the AppImage can stand in for it.
    reported="$(env -i HOME="$(mktemp -d)" PATH=/usr/bin:/bin LC_ALL=C.UTF-8 \
        APPIMAGE_EXTRACT_AND_RUN=1 QT_QPA_PLATFORM=offscreen \
        timeout 120 "${OUTPUT}" --version 2>&1 | tail -n1)"
    echo "    ${reported}"
    [[ "${reported}" == *"SpacePenguin ${VERSION}"* ]] ||
        die "the AppImage did not start correctly (expected \"SpacePenguin ${VERSION}\")"
fi

(cd "${OUTPUT_DIR}" && sha256sum "$(basename "${OUTPUT}")" > "$(basename "${OUTPUT}").sha256")

step "Created ${OUTPUT_DIR}/$(basename "${OUTPUT}") ($(du -h "${OUTPUT}" | cut -f1))"
echo "    Run it with: chmod +x $(basename "${OUTPUT}") && ./$(basename "${OUTPUT}")"
