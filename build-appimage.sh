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
for tool in cmake curl file sha256sum; do
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
QT_PREFIX="$(cd "${QT_PREFIX}" && pwd)"   # linuxdeploy runs elsewhere; a relative --qt would break

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

# Fixed releases, checked against their SHA-256 before every use: these tools run
# with full access to the build that produces the shipped AppImage, so a changed
# download must never be run. To update, pick new tags and record the new hashes.
LINUXDEPLOY_TAG="1-alpha-20251107-1"
LINUXDEPLOY_QT_TAG="1-alpha-20250213-1"
case "${ARCH}" in
    x86_64)
        LINUXDEPLOY_SHA256="c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"
        LINUXDEPLOY_QT_SHA256="15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724" ;;
    aarch64)
        LINUXDEPLOY_SHA256="620095110d693282b8ebeb244a95b5e911cf8f65f76c88b4b47d16ae6346fcff"
        LINUXDEPLOY_QT_SHA256="bf1c24aff6d749b5cf423afad6f15abd4440f81dec1aab95706b25f6667cdcf1" ;;
esac

TOOLS_DIR="${ROOT}/.cache/linuxdeploy"
mkdir -p "${TOOLS_DIR}"
# Downloads <name> from <url> unless a copy with the right hash is cached, and
# refuses to continue with anything that does not match <sha256>.
fetch() {
    local name="$1" url="$2" sha256="$3" file="${TOOLS_DIR}/$1"
    if [[ -f "${file}" ]] && echo "${sha256}  ${file}" | sha256sum --check --status; then
        return
    fi
    step "Downloading ${name}"
    rm -f "${file}" "${file}.part"
    curl -fSL --retry 3 -o "${file}.part" "${url}"
    if ! echo "${sha256}  ${file}.part" | sha256sum --check --status; then
        rm -f "${file}.part"
        die "${name} from ${url} does not have the expected SHA-256; refusing to run it"
    fi
    chmod +x "${file}.part"
    mv "${file}.part" "${file}"
}
fetch "linuxdeploy-${ARCH}.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/${LINUXDEPLOY_TAG}/linuxdeploy-${ARCH}.AppImage" \
    "${LINUXDEPLOY_SHA256}"
fetch "linuxdeploy-plugin-qt-${ARCH}.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/${LINUXDEPLOY_QT_TAG}/linuxdeploy-plugin-qt-${ARCH}.AppImage" \
    "${LINUXDEPLOY_QT_SHA256}"

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
# this Qt has them (linuxdeploy fails on a plugin that does not exist). Qt 6.10
# has one libqwayland.so; earlier versions split it into -egl and -generic, which
# distributions ship separately (qt6-wayland on Debian and Ubuntu).
QT_PLUGINS="$("${QMAKE}" -query QT_INSTALL_PLUGINS)"
extra_plugins=()
for plugin in libqwayland.so libqwayland-egl.so libqwayland-generic.so libqoffscreen.so; do
    [[ -f "${QT_PLUGINS}/platforms/${plugin}" ]] && extra_plugins+=("${plugin}")
done
if ! compgen -G "${QT_PLUGINS}/platforms/libqwayland*.so" >/dev/null; then
    echo "warning: this Qt has no Wayland platform plugin, so the AppImage will use XWayland" >&2
    echo "         on Wayland desktops; install qt6-wayland (or your distribution's equivalent) to bundle it" >&2
fi
[[ -f "${QT_PLUGINS}/platforms/libqoffscreen.so" ]] ||
    echo "warning: this Qt has no offscreen platform plugin" >&2
EXTRA_PLATFORM_PLUGINS="$(IFS=';'; echo "${extra_plugins[*]}")"
export EXTRA_PLATFORM_PLUGINS
# The Wayland plugin also needs Qt's Wayland shell plugins (xdg-shell is what opens
# a window), decorations and graphics integration. linuxdeploy's Qt plugin does not
# bring these for a client, so they are copied here; without them Qt falls back to
# XWayland on Wayland desktops.
wayland_deps=()
for folder in wayland-shell-integration wayland-decoration-client wayland-graphics-integration-client; do
    if [[ -d "${QT_PLUGINS}/${folder}" ]]; then
        mkdir -p "${APPDIR}/usr/plugins"
        rm -rf "${APPDIR}/usr/plugins/${folder}"
        cp -r "${QT_PLUGINS}/${folder}" "${APPDIR}/usr/plugins/${folder}"
        wayland_deps+=(--deploy-deps-only "${APPDIR}/usr/plugins/${folder}")
    fi
done
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
    ${wayland_deps[@]+"${wayland_deps[@]}"} \
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
