#!/usr/bin/env bash
# Installs SpacePenguin on Linux, for you alone: no root needed for SpacePenguin
# itself, and it appears in your app menu. In a terminal:
#
#   curl -fsSL https://raw.githubusercontent.com/developer1-vd/SpacePenguin/main/install.sh | bash
#
# or, with the script downloaded:
#
#   bash install.sh                          download the newest version and install it
#   bash install.sh SpacePenguin-*.AppImage  install an AppImage you already have
#   bash install.sh --uninstall              remove it again (asks about your data)
#   bash install.sh --uninstall --purge      remove it and your bookmarks, history and logins
#   bash install.sh --no-deps                do not install missing system libraries
#
# It downloads the AppImage of the newest release (or, before the first release,
# of the newest build of main), checks its SHA-256, and unpacks it into
# ~/.local/opt/spacepenguin: that starts faster than the AppImage, needs no FUSE,
# and takes no space twice. Run it again to update.
#
# The few libraries an AppImage takes from the system (graphics drivers, fonts, the
# display connection) are installed with apt, dnf, zypper or pacman when missing,
# which asks for your password. Works on Debian, Ubuntu, Mint, Fedora, openSUSE,
# Arch and their relatives, and in ChromeOS's Linux environment.
set -euo pipefail

REPO="developer1-vd/SpacePenguin"
APP_ID="org.spacepenguin.SpacePenguin"
DATA_HOME="${XDG_DATA_HOME:-${HOME}/.local/share}"
INSTALL_DIR="${HOME}/.local/opt/spacepenguin"
BIN_DIR="${HOME}/.local/bin"
LAUNCHER="${DATA_HOME}/applications/${APP_ID}.desktop"
ICON_THEME="${DATA_HOME}/icons/hicolor"
ICON_SIZES=(16 32 48 64 128 256 512)

ASSUME_YES=0
UNINSTALL=0
PURGE=0
DEPS=1
APPIMAGE=""

usage() {
    cat <<'EOF'
Usage: install.sh [options] [SpacePenguin-*.AppImage]

  (no file)        download the newest SpacePenguin and install it
  <file>           install this AppImage
  --uninstall      remove SpacePenguin; asks whether to delete your data too
  --purge          with --uninstall: also delete bookmarks, history, logins,
                   settings and cache, without asking
  --no-deps        do not install missing system libraries
  -y, --yes        do not ask anything
  -h, --help       show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -y|--yes) ASSUME_YES=1; shift ;;
        --uninstall) UNINSTALL=1; shift ;;
        --purge) PURGE=1; shift ;;
        --no-deps) DEPS=0; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
        *) APPIMAGE="$1"; shift ;;
    esac
done

step() { printf '\n==> %s\n' "$*"; }
die() { echo "error: $*" >&2; exit 1; }

# Tells the desktop the menu and its icons changed, so it shows them now.
refresh_menu() {
    if command -v update-desktop-database >/dev/null; then
        update-desktop-database "$(dirname "${LAUNCHER}")" >/dev/null 2>&1 || true
    fi
    if command -v gtk-update-icon-cache >/dev/null && [[ -f "${ICON_THEME}/index.theme" ]]; then
        gtk-update-icon-cache -q -t "${ICON_THEME}" >/dev/null 2>&1 || true
    fi
    touch "${ICON_THEME}" 2>/dev/null || true
    if command -v kbuildsycoca6 >/dev/null; then
        kbuildsycoca6 >/dev/null 2>&1 || true
    elif command -v kbuildsycoca5 >/dev/null; then
        kbuildsycoca5 >/dev/null 2>&1 || true
    fi
}

if [[ ${UNINSTALL} -eq 1 ]]; then
    if pgrep -x SpacePenguin >/dev/null 2>&1 || pgrep -f "${INSTALL_DIR}/" >/dev/null 2>&1; then
        echo "SpacePenguin is running; close it first, then run this again." >&2
        exit 1
    fi
    if [[ -e "${INSTALL_DIR}" || -e "${BIN_DIR}/spacepenguin" || -e "${LAUNCHER}" ]]; then
        rm -rf "${INSTALL_DIR}"
        rm -f "${BIN_DIR}/spacepenguin" "${LAUNCHER}"
        for size in "${ICON_SIZES[@]}"; do
            rm -f "${ICON_THEME}/${size}x${size}/apps/${APP_ID}.png"
        done
        refresh_menu
        echo "SpacePenguin is removed."
    else
        echo "SpacePenguin is not installed in ${INSTALL_DIR}."
        echo "(A .deb, .rpm, Flatpak or Arch package is removed with its package manager.)"
    fi

    # What it kept: bookmarks, history, saved logins, cookies, settings and cache.
    data=("${DATA_HOME}/SpacePenguin" "${XDG_CONFIG_HOME:-${HOME}/.config}/SpacePenguin"
          "${XDG_CACHE_HOME:-${HOME}/.cache}/SpacePenguin")
    existing=()
    for folder in "${data[@]}"; do
        [[ -e "${folder}" ]] && existing+=("${folder}")
    done
    if [[ ${#existing[@]} -eq 0 ]]; then
        exit 0
    fi
    if [[ ${PURGE} -eq 0 && ${ASSUME_YES} -eq 0 ]] && { : </dev/tty; } 2>/dev/null; then
        # The terminal, not stdin: this also runs as curl … | bash.
        printf 'Also delete your bookmarks, history, saved logins and settings? [y/N] ' >/dev/tty
        read -r answer </dev/tty || answer=""
        [[ "${answer}" =~ ^[Yy] ]] && PURGE=1
    fi
    if [[ ${PURGE} -eq 1 ]]; then
        rm -rf "${existing[@]}"
        echo "Your SpacePenguin data is deleted too."
    else
        echo "Your bookmarks, history and logins are kept in ${existing[0]}; installing again brings them back."
        echo "To delete them as well: bash install.sh --uninstall --purge"
    fi
    if [[ -e /etc/apparmor.d/spacepenguin-local ]]; then
        echo "The AppArmor profile for the sandbox stays; remove it with:"
        echo "    sudo packaging/linux/install-apparmor-profile.sh --remove"
    fi
    exit 0
fi

[[ "$(uname -s)" == Linux ]] || die "this installs the Linux version; use the .dmg or the Windows installer instead"
[[ "$(id -u)" -ne 0 ]] || echo "Note: installing for root. Run this as yourself to install it for your account."
ARCH="$(uname -m)"
case "${ARCH}" in
    x86_64|aarch64) ;;
    armv7l|armv8l) die "this system runs in 32-bit ARM mode, which QtWebEngine does not support" ;;
    *) die "there is no SpacePenguin build for ${ARCH}" ;;
esac

if [[ -e /dev/.cros_milestone ]]; then
    echo "Chromebook found (ChromeOS $(cat /dev/.cros_milestone 2>/dev/null || echo '?')), ${ARCH}."
fi

# ---- Libraries the AppImage takes from the system ---------------------------------
# Graphics drivers, fonts and the display connection cannot be bundled. Desktops
# have them; minimal systems and containers may not.

sudo=""
[[ "$(id -u)" -ne 0 ]] && sudo="sudo"

install_missing() {
    # $1: how to check one package, $2…: how to install, then -- and the packages
    local check="$1"; shift
    local install=()
    while [[ "$1" != "--" ]]; do install+=("$1"); shift; done
    shift
    local missing=()
    for package in "$@"; do
        ${check} "${package}" >/dev/null 2>&1 || missing+=("${package}")
    done
    [[ ${#missing[@]} -gt 0 ]] || return 0
    step "Installing ${#missing[@]} system libraries: ${missing[*]}"
    ${sudo} "${install[@]}" "${missing[@]}" && return 0
    # One unknown name fails the whole batch; then try them one at a time.
    local failed=()
    for package in "${missing[@]}"; do
        ${sudo} "${install[@]}" "${package}" >/dev/null 2>&1 || failed+=("${package}")
    done
    [[ ${#failed[@]} -eq 0 ]] ||
        echo "warning: could not install ${failed[*]}; SpacePenguin may not start without them" >&2
}

if [[ ${DEPS} -eq 1 ]]; then
    if command -v apt-get >/dev/null; then
        packages=(libegl1 libgl1 libopengl0 libfontconfig1 libfreetype6 libx11-6 libx11-xcb1
                  libxkbcommon0 libxkbcommon-x11-0 libwayland-client0 libwayland-egl1
                  libwayland-cursor0 libxcb-cursor0 libasound2 libnss3 libxshmfence1
                  libgbm1 libdrm2 libglib2.0-0 libgpg-error0 libcom-err2 libgmp10 libharfbuzz0b fonts-dejavu-core
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
        apt_installed() { dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q "ok installed"; }
        apt_install() { ${sudo} apt-get update -qq || true
                        ${sudo} env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends "$@" >/dev/null; }
        missing=()
        for package in "${available[@]}"; do apt_installed "${package}" || missing+=("${package}"); done
        if [[ ${#missing[@]} -gt 0 ]]; then
            step "Installing ${#missing[@]} system libraries: ${missing[*]}"
            apt_install "${missing[@]}" ||
                echo "warning: could not install them; SpacePenguin may not start until they are installed" >&2
        fi
    elif command -v dnf >/dev/null; then
        install_missing "rpm -q" dnf install -y -q -- \
            mesa-libEGL mesa-libGL libglvnd-opengl fontconfig freetype libX11 libX11-xcb \
            libxkbcommon libxkbcommon-x11 libwayland-client libwayland-egl libwayland-cursor \
            xcb-util-cursor alsa-lib nss libxshmfence mesa-libgbm libdrm glib2 libgpg-error libcom_err gmp \
            dejavu-sans-fonts \
            ca-certificates curl
    elif command -v zypper >/dev/null; then
        install_missing "rpm -q" zypper --non-interactive --quiet install -- \
            libglvnd fontconfig libfreetype6 libX11-6 libX11-xcb1 \
            libxkbcommon0 libxkbcommon-x11-0 libwayland-client0 libwayland-egl1 \
            libwayland-cursor0 libxcb-cursor0 libasound2 mozilla-nss libxshmfence1 libgbm1 \
            libdrm2 libglib-2_0-0 libgpg-error0 libcom_err2 libgmp10 dejavu-fonts ca-certificates curl
    elif command -v pacman >/dev/null; then
        install_missing "pacman -Q" pacman -S --needed --noconfirm -- \
            libglvnd fontconfig freetype2 libx11 libxkbcommon libxkbcommon-x11 wayland \
            xcb-util-cursor alsa-lib nss libxshmfence mesa libdrm glib2 libgpg-error e2fsprogs gmp ttf-dejavu \
            ca-certificates curl
    else
        echo "Unknown package manager: if SpacePenguin does not start, install your system's OpenGL, fontconfig, X11/Wayland and NSS libraries."
    fi
fi

# ---- Get the AppImage -------------------------------------------------------------

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

appimage_url() {
    # $1: a release API address. Prints the AppImage for this machine, if it has one.
    # A missing release is a 404, which is not an error here: say nothing.
    { curl -fsSL "$1" 2>/dev/null || true; } |
        sed -n "s/.*\"browser_download_url\": *\"\([^\"]*-${ARCH}\.AppImage\)\".*/\1/p" | head -n1
}

if [[ -z "${APPIMAGE}" ]]; then
    command -v curl >/dev/null || die "curl is missing; install it with your package manager"
    step "Looking up the newest version"
    url="$(appimage_url "https://api.github.com/repos/${REPO}/releases/latest")"
    if [[ -z "${url}" ]]; then
        # Before the first release: the build of main that CI publishes as "nightly".
        url="$(appimage_url "https://api.github.com/repos/${REPO}/releases/tags/nightly")"
        [[ -z "${url}" ]] || echo "No release yet: installing the newest build of main (nightly)."
    fi
    [[ -n "${url}" ]] || die "no ${ARCH} AppImage is published yet; download one by hand and run: bash install.sh <file>"
    step "Downloading $(basename "${url}")"
    curl -fL --retry 3 -o "${WORK}/SpacePenguin.AppImage" "${url}"
    # Every AppImage is published with a .sha256 beside it; nothing is installed
    # or run without it matching.
    curl -fsSL --retry 3 -o "${WORK}/sum" "${url}.sha256" ||
        die "could not download $(basename "${url}").sha256 to check the download; try again"
    expected="$(cut -d' ' -f1 "${WORK}/sum")"
    [[ "${expected}" =~ ^[0-9a-f]{64}$ ]] || die "$(basename "${url}").sha256 is not a SHA-256 checksum"
    actual="$(sha256sum "${WORK}/SpacePenguin.AppImage" | cut -d' ' -f1)"
    [[ "${expected}" == "${actual}" ]] || die "the download is damaged (checksum mismatch); try again"
    APPIMAGE="${WORK}/SpacePenguin.AppImage"
else
    [[ -f "${APPIMAGE}" ]] || die "${APPIMAGE} does not exist. On a Chromebook, files from the Files app must be shared with Linux first (right-click › Share with Linux) or moved to \"Linux files\"."
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
mkdir -p "$(dirname "${INSTALL_DIR}")" "${BIN_DIR}" "$(dirname "${LAUNCHER}")"
mv "${WORK}/squashfs-root" "${INSTALL_DIR}"

cat > "${BIN_DIR}/spacepenguin" <<EOF
#!/bin/sh
exec "${INSTALL_DIR}/AppRun" "\$@"
EOF
chmod +x "${BIN_DIR}/spacepenguin"

# The penguin in every size the app ships, where icon themes look for it.
for size in "${ICON_SIZES[@]}"; do
    icon="${INSTALL_DIR}/usr/share/icons/hicolor/${size}x${size}/apps/${APP_ID}.png"
    if [[ -f "${icon}" ]]; then
        mkdir -p "${ICON_THEME}/${size}x${size}/apps"
        cp "${icon}" "${ICON_THEME}/${size}x${size}/apps/${APP_ID}.png"
    fi
done
# The menu entry names the icon by its full path: some menus (and ChromeOS's
# launcher) never look in ~/.local/share/icons, or not until the next login.
menu_icon="${ICON_THEME}/256x256/apps/${APP_ID}.png"
[[ -f "${menu_icon}" ]] || menu_icon="${APP_ID}"

# The app menu entry, pointing at this copy. ChromeOS adds it to its own launcher.
sed -e "s|^Exec=SpacePenguin|Exec=${BIN_DIR}/spacepenguin|" \
    -e "s|^TryExec=.*|TryExec=${BIN_DIR}/spacepenguin|" \
    -e "s|^Icon=.*|Icon=${menu_icon}|" \
    "${INSTALL_DIR}/usr/share/applications/${APP_ID}.desktop" > "${LAUNCHER}"
refresh_menu

output="$(QT_QPA_PLATFORM=offscreen "${BIN_DIR}/spacepenguin" --version 2>&1 || true)"
version="$(printf '%s\n' "${output}" | grep '^SpacePenguin' | tail -n1 || true)"
if [[ "${version}" != SpacePenguin* ]]; then
    printf '%s\n' "${output}" | tail -n 15 >&2
    die "SpacePenguin was installed but does not start (above is why); run ${BIN_DIR}/spacepenguin in a terminal to try again"
fi

step "${version} is installed"
echo "    Open it from your app menu, or run: spacepenguin"
case ":${PATH}:" in
    *":${BIN_DIR}:"*) ;;
    *) echo "    (${BIN_DIR} is not on your PATH yet; it is after you log in again, or run ${BIN_DIR}/spacepenguin)" ;;
esac
if [[ -e /dev/.cros_milestone ]]; then
    echo "    On a Chromebook with 4 GB of memory it limits itself to a few processes, and"
    echo "    draws without the GPU where ChromeOS gives Linux apps none; about:version"
    echo "    shows what it chose."
fi
echo "    Update by running this again; remove it with: bash uninstall.sh (or install.sh --uninstall)"
