#!/usr/bin/env bash
# Installs SpacePenguin on a Chromebook, in ChromeOS's Linux environment.
#
# Turn on Linux first: Settings › About ChromeOS › Developers › Linux development
# environment. (On a school-managed Chromebook the school decides whether that
# setting is there; if it is missing or greyed out, only the school's admin can
# allow it.) Then, in the Terminal app:
#
#   bash install.sh                      download the newest release and install it
#   bash install.sh SpacePenguin-*.AppImage   install an AppImage you already have
#   bash install.sh --uninstall          remove it again (keeps your bookmarks)
#
# SpacePenguin then appears in the ChromeOS launcher under "Linux apps".
#
# The AppImage is unpacked into ~/.local/opt/spacepenguin rather than run as it
# is: it starts faster, needs no FUSE, and takes no space twice. Nothing goes
# outside your home folder apart from a few system libraries installed with apt.
# Works on any Debian or Ubuntu system, not only on ChromeOS.
set -euo pipefail

REPO="developer1-vd/SpacePenguin"
APP_ID="org.spacepenguin.SpacePenguin"
DATA_HOME="${XDG_DATA_HOME:-${HOME}/.local/share}"
INSTALL_DIR="${HOME}/.local/opt/spacepenguin"
BIN_DIR="${HOME}/.local/bin"
LAUNCHER="${DATA_HOME}/applications/${APP_ID}.desktop"
ICON_DIR="${DATA_HOME}/icons/hicolor/256x256/apps"

ASSUME_YES=0
UNINSTALL=0
APPIMAGE=""

usage() { sed -n '2,19s/^# \{0,1\}//p' "$0"; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        -y|--yes) ASSUME_YES=1; shift ;;
        --uninstall) UNINSTALL=1; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "Unknown option: $1" >&2; exit 2 ;;
        *) APPIMAGE="$1"; shift ;;
    esac
done

step() { printf '\n==> %s\n' "$*"; }
die() { echo "error: $*" >&2; exit 1; }

if [[ ${UNINSTALL} -eq 1 ]]; then
    rm -rf "${INSTALL_DIR}"
    rm -f "${BIN_DIR}/spacepenguin" "${LAUNCHER}" "${ICON_DIR}/${APP_ID}.png"
    echo "SpacePenguin is removed. Your bookmarks and history are still in ~/.local/share/SpacePenguin."
    exit 0
fi

[[ "$(uname -s)" == Linux ]] || die "this installs the Linux version; use the .dmg or the Windows installer instead"
ARCH="$(uname -m)"
case "${ARCH}" in
    x86_64|aarch64) ;;
    armv7l|armv8l)
        die "this Chromebook's Linux runs in 32-bit ARM mode, which QtWebEngine does not support" ;;
    *) die "there is no SpacePenguin build for ${ARCH}" ;;
esac

if [[ -e /dev/.cros_milestone ]]; then
    echo "Chromebook found (ChromeOS $(cat /dev/.cros_milestone 2>/dev/null || echo '?')), ${ARCH}."
fi

# ---- Libraries the AppImage takes from the system ---------------------------------
# Graphics drivers, fonts and the display connection cannot be bundled; ChromeOS's
# Linux has most of them, but not all.

if command -v apt-get >/dev/null; then
    packages=(libegl1 libgl1 libopengl0 libfontconfig1 libfreetype6 libx11-6 libx11-xcb1
              libxkbcommon0 libxkbcommon-x11-0 libwayland-client0 libwayland-egl1
              libwayland-cursor0 libxcb-cursor0 libasound2 libnss3 libxshmfence1
              libgbm1 libdrm2 libglib2.0-0 libgpg-error0 libharfbuzz0b fonts-dejavu-core
              ca-certificates curl)
    # Ubuntu 24.04 and later renamed some of these with a t64 suffix.
    available=()
    for package in "${packages[@]}"; do
        if apt-cache show "${package}" >/dev/null 2>&1; then
            available+=("${package}")
        elif apt-cache show "${package}t64" >/dev/null 2>&1; then
            available+=("${package}t64")
        fi
    done
    missing=()
    for package in "${available[@]}"; do
        dpkg-query -W -f='${Status}' "${package}" 2>/dev/null | grep "ok installed" >/dev/null ||
            missing+=("${package}")
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        step "Installing ${#missing[@]} system libraries: ${missing[*]}"
        sudo=""
        [[ "$(id -u)" -ne 0 ]] && sudo="sudo"
        ${sudo} apt-get update -qq || true
        ${sudo} env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
            "${missing[@]}" >/dev/null
    fi
fi

# ---- Get the AppImage -------------------------------------------------------------

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

if [[ -z "${APPIMAGE}" ]]; then
    command -v curl >/dev/null || die "curl is missing; run: sudo apt install curl"
    step "Looking up the newest release"
    url="$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" |
        sed -n "s/.*\"browser_download_url\": *\"\([^\"]*-${ARCH}\.AppImage\)\".*/\1/p" | head -n1)"
    [[ -n "${url}" ]] || die "no ${ARCH} AppImage in the newest release of ${REPO}; download one by hand and run: bash $0 <file>"
    step "Downloading $(basename "${url}")"
    curl -fL --retry 3 -o "${WORK}/SpacePenguin.AppImage" "${url}"
    if curl -fsSL -o "${WORK}/sum" "${url}.sha256" 2>/dev/null; then
        expected="$(cut -d' ' -f1 "${WORK}/sum")"
        actual="$(sha256sum "${WORK}/SpacePenguin.AppImage" | cut -d' ' -f1)"
        [[ "${expected}" == "${actual}" ]] || die "the download is damaged (checksum mismatch); try again"
    fi
    APPIMAGE="${WORK}/SpacePenguin.AppImage"
else
    [[ -f "${APPIMAGE}" ]] || die "${APPIMAGE} does not exist. Files from the ChromeOS Files app must be shared with Linux first (right-click › Share with Linux) or moved to \"Linux files\"."
    cp "${APPIMAGE}" "${WORK}/SpacePenguin.AppImage"
    APPIMAGE="${WORK}/SpacePenguin.AppImage"
fi

case "$(head -c 20 "${APPIMAGE}" | od -An -tx1 | tr -d ' \n')" in
    7f454c46*) ;;
    *) die "that file is not an AppImage" ;;
esac

# ---- Unpack and register ----------------------------------------------------------

step "Installing into ${INSTALL_DIR}"
chmod +x "${APPIMAGE}"
(cd "${WORK}" && "${APPIMAGE}" --appimage-extract >/dev/null)
[[ -x "${WORK}/squashfs-root/AppRun" ]] || die "could not unpack the AppImage"
if [[ -e "${INSTALL_DIR}" && ${ASSUME_YES} -eq 0 ]]; then
    echo "Replacing the SpacePenguin already installed there (your data is kept)."
fi
rm -rf "${INSTALL_DIR}"
mkdir -p "$(dirname "${INSTALL_DIR}")" "${BIN_DIR}" "$(dirname "${LAUNCHER}")" "${ICON_DIR}"
mv "${WORK}/squashfs-root" "${INSTALL_DIR}"

cat > "${BIN_DIR}/spacepenguin" <<EOF
#!/bin/sh
exec "${INSTALL_DIR}/AppRun" "\$@"
EOF
chmod +x "${BIN_DIR}/spacepenguin"

icon="$(find "${INSTALL_DIR}/usr/share/icons" -path "*256x256*" -name "${APP_ID}.png" 2>/dev/null | head -n1)"
[[ -n "${icon}" ]] && cp "${icon}" "${ICON_DIR}/${APP_ID}.png"

# The launcher entry, pointing at this copy. ChromeOS adds it to its own launcher.
sed -e "s|^Exec=SpacePenguin|Exec=${BIN_DIR}/spacepenguin|" \
    -e "s|^TryExec=.*|TryExec=${BIN_DIR}/spacepenguin|" \
    "${INSTALL_DIR}/usr/share/applications/${APP_ID}.desktop" > "${LAUNCHER}"
if command -v update-desktop-database >/dev/null; then
    update-desktop-database "$(dirname "${LAUNCHER}")" >/dev/null 2>&1 || true
fi

version="$(QT_QPA_PLATFORM=offscreen "${BIN_DIR}/spacepenguin" --version 2>/dev/null | tail -n1 || true)"
[[ "${version}" == SpacePenguin* ]] || die "SpacePenguin was installed but does not start; run ${BIN_DIR}/spacepenguin in the Terminal to see why"

step "${version} is installed"
cat <<EOF
    Open it from the launcher (Linux apps › SpacePenguin), or run: spacepenguin
    On a Chromebook with 4 GB of memory it limits itself to a few processes, and
    it draws without the GPU where ChromeOS gives Linux apps none; about:version
    shows what it chose.
    Remove it with: bash $0 --uninstall
EOF
