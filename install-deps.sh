#!/usr/bin/env bash
# Install everything needed to build, test and package SpacePenguin on Linux or
# macOS. On Windows use install-deps.ps1; on NixOS use `nix-shell` (shell.nix).
# Needs bash: on Alpine run `apk add bash` and on Void `xbps-install bash` first.
#
#   ./install-deps.sh              compiler, CMake, libraries and Qt
#   ./install-deps.sh --help       all options
#
# SpacePenguin needs Qt 6.8 or newer with QtWebEngine. Many distributions ship an
# older Qt, so by default ("--qt auto") Qt comes from the official Qt mirrors
# through aqtinstall and goes into .qt/ inside the repository, where CMake finds
# it with no extra flags. Distributions whose own Qt is new enough, and musl
# distributions that cannot run Qt's prebuilt glibc binaries, use the system Qt.
set -euo pipefail

QT_VERSION="6.8.3"
QT_MODULES=(qtwebengine qtwebchannel qtpositioning qtpdf qtserialport)
MIN_QT="6.8.0"
MIN_CMAKE="3.21"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT_DIR="${ROOT}/.qt"
QT_SOURCE="auto"   # auto | system | bundled
PACKAGING=1
ASSUME_YES=0
DRY_RUN=0

usage() {
    cat <<EOF
Usage: ./install-deps.sh [options]

  --qt <auto|system|bundled>
                        where Qt comes from (default auto):
                          system   the distribution's packages (needs Qt >= ${MIN_QT})
                          bundled  Qt ${QT_VERSION} from the Qt mirrors into --qt-dir
                          auto     system when the distribution ships Qt >= ${MIN_QT}
                                   or uses musl, bundled otherwise
  --system-qt           same as --qt system
  --qt-version <x.y.z>  bundled Qt version (default ${QT_VERSION})
  --qt-dir <dir>        where to put the bundled Qt (default .qt in the repository)
  --no-packaging        skip the tools only needed to build packages
  -y, --yes             do not ask before installing system packages
  --dry-run             print the commands instead of running them
  -h, --help            show this help

Package managers: apt (Debian, Ubuntu, Mint, Pop!_OS, elementary, Kali, …),
dnf/yum (Fedora, RHEL, CentOS Stream, Rocky, AlmaLinux, Nobara, …), pacman (Arch,
Manjaro, EndeavourOS, CachyOS, …), zypper (openSUSE Tumbleweed/Leap, SLE), apk
(Alpine, postmarketOS), xbps (Void), emerge (Gentoo, Funtoo), and Homebrew on
macOS. NixOS: run nix-shell in the repository instead.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --qt) QT_SOURCE="$2"; shift 2 ;;
        --system-qt) QT_SOURCE="system"; shift ;;
        --bundled-qt) QT_SOURCE="bundled"; shift ;;
        --qt-version) QT_VERSION="$2"; shift 2 ;;
        --qt-dir) mkdir -p "$2"; QT_DIR="$(cd "$2" && pwd)"; shift 2 ;;
        --no-packaging) PACKAGING=0; shift ;;
        -y|--yes) ASSUME_YES=1; shift ;;
        --dry-run) DRY_RUN=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done
case "${QT_SOURCE}" in auto|system|bundled) ;; *) echo "--qt must be auto, system or bundled" >&2; exit 2 ;; esac

step() { printf '\n==> %s\n' "$*"; }
note() { printf '    %s\n' "$*"; }
run() {
    printf '    $ %s\n' "$*"
    [[ ${DRY_RUN} -eq 1 ]] || "$@"
}
die() { echo "error: $*" >&2; exit 1; }

# True when version $1 >= $2.
version_ge() { [[ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n1)" == "$2" ]]; }

confirm() {
    [[ ${ASSUME_YES} -eq 1 || ${DRY_RUN} -eq 1 ]] && return 0
    local answer
    read -r -p "$1 [Y/n] " answer
    [[ -z "${answer}" || "${answer}" =~ ^[Yy] ]]
}

SUDO=()
need_root() {
    if [[ $(id -u) -ne 0 ]]; then
        if command -v sudo >/dev/null; then SUDO=(sudo)
        elif command -v doas >/dev/null; then SUDO=(doas)
        else die "installing packages needs root, sudo or doas"
        fi
    fi
}

# ---- Which system ----------------------------------------------------------------

OS="$(uname -s)"
MANAGER=""
DISTRO_NAME=""
IS_MUSL=0
RHEL_LIKE=0

detect_linux() {
    [[ -r /etc/os-release ]] || die "cannot identify this Linux distribution (no /etc/os-release)"
    # shellcheck disable=SC1091
    . /etc/os-release
    DISTRO_NAME="${PRETTY_NAME:-${ID:-Linux}}"
    local family=" ${ID:-} ${ID_LIKE:-} "

    # grep without -q reads all its input: with pipefail, an early exit would
    # fail the writer with SIGPIPE and make the whole test false.
    if command -v ldd >/dev/null && ldd --version 2>&1 | grep -i musl >/dev/null; then
        IS_MUSL=1
    elif [[ -e /lib/ld-musl-x86_64.so.1 || -e /lib/ld-musl-aarch64.so.1 ]]; then
        IS_MUSL=1
    fi
    [[ "${family}" =~ \ (rhel|centos|rocky|almalinux|ol)\  ]] && RHEL_LIKE=1

    # Prefer the manager that matches the distribution family; fall back to
    # whichever one is installed.
    if [[ "${family}" =~ \ (debian|ubuntu)\  ]] && command -v apt-get >/dev/null; then MANAGER=apt
    elif [[ "${family}" =~ \ (fedora|rhel|centos)\  ]] && command -v dnf >/dev/null; then MANAGER=dnf
    elif [[ "${family}" =~ \ (arch)\  ]] && command -v pacman >/dev/null; then MANAGER=pacman
    elif [[ "${family}" =~ \ (suse|opensuse)\  ]] && command -v zypper >/dev/null; then MANAGER=zypper
    elif [[ "${family}" =~ \ (alpine)\  ]] && command -v apk >/dev/null; then MANAGER=apk
    elif [[ "${family}" =~ \ (void)\  ]] && command -v xbps-install >/dev/null; then MANAGER=xbps
    elif [[ "${family}" =~ \ (gentoo)\  ]] && command -v emerge >/dev/null; then MANAGER=emerge
    elif [[ "${family}" =~ \ (nixos)\  ]]; then MANAGER=nix
    else
        local candidate
        for candidate in apt-get dnf yum pacman zypper apk xbps-install emerge; do
            if command -v "${candidate}" >/dev/null; then
                case "${candidate}" in
                    apt-get) MANAGER=apt ;;
                    yum) MANAGER=dnf ;;
                    xbps-install) MANAGER=xbps ;;
                    *) MANAGER="${candidate}" ;;
                esac
                break
            fi
        done
    fi
    [[ -n "${MANAGER}" ]] || die "no supported package manager found. Install by hand: a C++20 compiler, CMake ${MIN_CMAKE}+, Ninja, Git, Python 3, and the OpenGL, xkbcommon, xcb-util, NSS, ALSA and fontconfig libraries; then run with --qt bundled, or install Qt ${MIN_QT}+ with QtWebEngine yourself."
}

# ---- Package lists ---------------------------------------------------------------
# Each manager installs: the toolchain, the libraries QtWebEngine needs at build and
# run time, optionally the packaging checks, and optionally the system Qt.

QT_FROM_SYSTEM=0

apt_installable() {
    apt-cache policy "$1" 2>/dev/null | grep '^ *Candidate: *[0-9]' >/dev/null
}

packages_apt() {
    PKGS=(build-essential cmake ninja-build git curl ca-certificates python3 python3-venv
          libgl1-mesa-dev libegl1 libxkbcommon-dev libxkbcommon-x11-0 libfontconfig1
          libdbus-1-3 libxcb-cursor0 libxcb-icccm4 libxcb-keysyms1 libxcb-shape0 libxcb-xkb1
          libxcb-render-util0 libxcb-image0 libnss3 libxcomposite1 libxdamage1 libxrandr2
          libxtst6 libxkbfile1)
    # Libraries Qt's prebuilt binaries link against that minimal systems lack.
    PKGS+=(libgssapi-krb5-2 libgbm1 libxshmfence1 libfreetype6 libdrm2 libx11-6 libx11-xcb1
           libxext6 libxfixes3 libxi6 libxrender1 libexpat1
           libwayland-client0 libwayland-cursor0 libwayland-egl1 libcups2)
    # Renamed for the 64-bit time_t transition (Ubuntu 24.04+, Debian 13+). Older
    # releases still know the new names, as references without a candidate.
    local library
    for library in libasound2 libglib2.0-0; do
        if apt_installable "${library}t64"; then PKGS+=("${library}t64"); else PKGS+=("${library}"); fi
    done
    [[ ${PACKAGING} -eq 1 ]] && PKGS+=(desktop-file-utils appstream file dpkg-dev)
    [[ ${QT_FROM_SYSTEM} -eq 1 ]] && PKGS+=(qt6-base-dev qt6-webengine-dev qt6-tools-dev
                                           qt6-base-dev-tools libgl-dev)
    return 0
}
system_qt_version_apt() {
    apt-cache policy qt6-webengine-dev 2>/dev/null |
        sed -n 's/^ *Candidate: *\([0-9]*:\)\{0,1\}\([0-9][0-9.]*\).*/\2/p'
}
install_apt() {
    run ${SUDO[@]+"${SUDO[@]}"} apt-get update
    packages_apt
    run ${SUDO[@]+"${SUDO[@]}"} env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${PKGS[@]}"
}

packages_dnf() {
    PKGS=(gcc-c++ make cmake ninja-build git curl python3 mesa-libGL-devel mesa-libEGL
          libxkbcommon-devel libxkbcommon-x11 fontconfig dbus-libs xcb-util-cursor xcb-util-wm
          xcb-util-keysyms xcb-util-renderutil xcb-util-image nss libXcomposite libXdamage
          libXrandr libXtst libxkbfile alsa-lib glib2 krb5-libs mesa-libgbm libxshmfence
          freetype libdrm libX11 libX11-xcb libXext libXfixes libXi libXrender expat
          libwayland-client libwayland-cursor libwayland-egl cups-libs)
    [[ ${PACKAGING} -eq 1 ]] && PKGS+=(desktop-file-utils appstream file rpm-build)
    [[ ${QT_FROM_SYSTEM} -eq 1 ]] && PKGS+=(qt6-qtbase-devel qt6-qtwebengine-devel qt6-qttools-devel)
    return 0
}
system_qt_version_dnf() {
    local tool=dnf; command -v dnf >/dev/null || tool=yum
    "${tool}" -q repoquery --qf '%{version}\n' qt6-qtwebengine-devel 2>/dev/null | sort -V | tail -n1
}
install_dnf() {
    local tool=dnf; command -v dnf >/dev/null || tool=yum
    if [[ ${RHEL_LIKE} -eq 1 ]]; then
        # Ninja and the xcb-util packages live in CodeReady Builder and EPEL.
        run ${SUDO[@]+"${SUDO[@]}"} "${tool}" install -y dnf-plugins-core epel-release ||
            note "could not add EPEL; enable it by hand if packages are missing"
        run ${SUDO[@]+"${SUDO[@]}"} "${tool}" config-manager --set-enabled crb ||
            run ${SUDO[@]+"${SUDO[@]}"} "${tool}" config-manager --set-enabled powertools || true
    fi
    packages_dnf
    run ${SUDO[@]+"${SUDO[@]}"} "${tool}" install -y "${PKGS[@]}"
}

packages_pacman() {
    PKGS=(base-devel cmake ninja git curl python mesa libxkbcommon libxkbcommon-x11 fontconfig
          dbus xcb-util-cursor xcb-util-wm xcb-util-keysyms xcb-util-renderutil xcb-util-image
          nss libxcomposite libxdamage libxrandr libxtst libxkbfile alsa-lib glib2 krb5
          libxshmfence freetype2 libdrm libx11 libxext libxfixes libxi libxrender expat wayland libcups)
    [[ ${PACKAGING} -eq 1 ]] && PKGS+=(desktop-file-utils appstream file)
    [[ ${QT_FROM_SYSTEM} -eq 1 ]] && PKGS+=(qt6-base qt6-webengine qt6-tools)
    return 0
}
system_qt_version_pacman() {
    pacman -Si qt6-webengine 2>/dev/null | sed -n 's/^Version *: *\([0-9][0-9.]*\).*/\1/p'
}
install_pacman() {
    packages_pacman
    # A full upgrade: Arch does not support partial upgrades.
    run ${SUDO[@]+"${SUDO[@]}"} pacman -Syu --needed --noconfirm "${PKGS[@]}"
}

packages_zypper() {
    PKGS=(gcc-c++ make cmake ninja git curl python3 Mesa-libGL-devel Mesa-libEGL1
          libxkbcommon-devel libxkbcommon-x11-0 fontconfig dbus-1 libxcb-cursor0 libxcb-icccm4
          libxcb-keysyms1 libxcb-render-util0 libxcb-image0 mozilla-nss libXcomposite1
          libXdamage1 libXrandr2 libXtst6 libxkbfile1 alsa libglib-2_0-0 libgthread-2_0-0 krb5
          libgbm1 libxshmfence1 libfreetype6 libdrm2 libX11-6 libX11-xcb1 libXext6 libXfixes3 libXi6
          libXrender1 libexpat1 libwayland-client0 libwayland-cursor0 libwayland-egl1 libcups2)
    [[ ${PACKAGING} -eq 1 ]] && PKGS+=(desktop-file-utils AppStream file rpm-build)
    [[ ${QT_FROM_SYSTEM} -eq 1 ]] && PKGS+=(qt6-base-devel qt6-webenginewidgets-devel qt6-tools-devel)
    return 0
}
system_qt_version_zypper() {
    zypper --non-interactive --quiet info qt6-webenginewidgets-devel 2>/dev/null |
        sed -n 's/^Version *: *\([0-9][0-9.]*\).*/\1/p'
}
install_zypper() {
    packages_zypper
    run ${SUDO[@]+"${SUDO[@]}"} zypper --non-interactive --gpg-auto-import-keys refresh
    run ${SUDO[@]+"${SUDO[@]}"} zypper --non-interactive install --no-recommends "${PKGS[@]}"
}

packages_apk() {
    PKGS=(build-base cmake samurai git curl python3 bash mesa-dev libxkbcommon-dev fontconfig
          dbus-libs xcb-util-cursor xcb-util-wm xcb-util-keysyms xcb-util-renderutil
          xcb-util-image nss libxcomposite libxdamage libxrandr libxtst libxkbfile alsa-lib
          wayland-libs-client wayland-libs-cursor wayland-libs-egl cups-libs)
    [[ ${PACKAGING} -eq 1 ]] && PKGS+=(desktop-file-utils appstream file)
    [[ ${QT_FROM_SYSTEM} -eq 1 ]] && PKGS+=(qt6-qtbase-dev qt6-qtwebengine-dev qt6-qttools-dev)
    return 0
}
system_qt_version_apk() {
    apk policy qt6-qtwebengine-dev 2>/dev/null | sed -n '2s/^ *\([0-9][0-9.]*\).*/\1/p'
}
install_apk() {
    packages_apk
    run ${SUDO[@]+"${SUDO[@]}"} apk add --no-cache "${PKGS[@]}"
}

packages_xbps() {
    PKGS=(base-devel cmake ninja git curl python3 MesaLib-devel libxkbcommon-devel fontconfig
          dbus-libs xcb-util-cursor xcb-util-wm xcb-util-keysyms xcb-util-renderutil
          xcb-util-image nss libXcomposite libXdamage libXrandr libXtst libxkbfile alsa-lib glib
          mit-krb5-libs libgbm libxshmfence freetype libdrm libX11 libXext libXfixes libXi
          libXrender expat wayland libcups)
    [[ ${PACKAGING} -eq 1 ]] && PKGS+=(desktop-file-utils AppStream file)
    [[ ${QT_FROM_SYSTEM} -eq 1 ]] && PKGS+=(qt6-base-devel qt6-webengine-devel qt6-tools-devel)
    return 0
}
system_qt_version_xbps() {
    xbps-query -R --property pkgver qt6-webengine-devel 2>/dev/null | sed 's/^qt6-webengine-devel-//; s/_.*//'
}
install_xbps() {
    packages_xbps
    run ${SUDO[@]+"${SUDO[@]}"} xbps-install -Sy xbps
    run ${SUDO[@]+"${SUDO[@]}"} xbps-install -Sy "${PKGS[@]}"
}

packages_emerge() {
    PKGS=(dev-build/cmake dev-build/ninja dev-vcs/git net-misc/curl dev-lang/python
          media-libs/mesa x11-libs/libxkbcommon media-libs/fontconfig sys-apps/dbus
          x11-libs/xcb-util-cursor x11-libs/xcb-util-wm x11-libs/xcb-util-keysyms
          x11-libs/xcb-util-renderutil x11-libs/xcb-util-image dev-libs/nss
          x11-libs/libXcomposite x11-libs/libXdamage x11-libs/libXrandr x11-libs/libXtst
          x11-libs/libxkbfile media-libs/alsa-lib dev-libs/glib app-crypt/mit-krb5
          dev-libs/wayland net-print/cups)
    [[ ${PACKAGING} -eq 1 ]] && PKGS+=(dev-util/desktop-file-utils dev-libs/appstream sys-apps/file)
    [[ ${QT_FROM_SYSTEM} -eq 1 ]] && PKGS+=(dev-qt/qtbase:6 dev-qt/qtwebengine:6 dev-qt/qttools:6)
    return 0
}
system_qt_version_emerge() {
    # equery is optional; without it, trust the tree (Gentoo stable has Qt 6.8+).
    if command -v equery >/dev/null; then
        # shellcheck disable=SC2016  # $version is equery's own format token
        equery -q list -p -F '$version' dev-qt/qtwebengine 2>/dev/null | sort -V | tail -n1
    else
        echo "${MIN_QT}"
    fi
}
install_emerge() {
    packages_emerge
    note "QtWebEngine compiles from source on Gentoo and can take hours."
    run ${SUDO[@]+"${SUDO[@]}"} emerge --ask=n --noreplace --verbose "${PKGS[@]}"
}

install_macos() {
    if ! xcode-select -p >/dev/null 2>&1; then
        step "Installing the Xcode command line tools"
        run xcode-select --install || true
        die "finish the Xcode command line tools installer, then run this script again"
    fi
    command -v brew >/dev/null ||
        die "Homebrew is required: https://brew.sh — install it, then run this script again"
    PKGS=(cmake ninja python@3)
    [[ ${PACKAGING} -eq 1 ]] && PKGS+=(create-dmg)
    [[ ${QT_FROM_SYSTEM} -eq 1 ]] && PKGS+=(qt)
    run brew install "${PKGS[@]}"
}
system_qt_version_brew() {
    brew info --json=v2 qt 2>/dev/null | python3 -c 'import json,sys; print(json.load(sys.stdin)["formulae"][0]["versions"]["stable"])' 2>/dev/null
}

# ---- Decide where Qt comes from -------------------------------------------------

case "${OS}" in
    Linux) detect_linux ;;
    Darwin) MANAGER=brew; DISTRO_NAME="macOS $(sw_vers -productVersion 2>/dev/null || true)" ;;
    *) die "unsupported system ${OS}; on Windows run install-deps.ps1" ;;
esac

if [[ "${MANAGER}" == nix ]]; then
    step "NixOS detected"
    note "Nix provides everything; nothing needs installing. In the repository:"
    note "    nix build        # the browser, in ./result/bin/SpacePenguin"
    note "    nix develop      # a shell with the toolchain and Qt, then: cmake -B build"
    note "Without flakes: nix-build and nix-shell do the same."
    exit 0
fi

ARCH="$(uname -m)"
BUNDLED_POSSIBLE=1
BUNDLED_REASON=""
if [[ ${IS_MUSL} -eq 1 ]]; then
    BUNDLED_POSSIBLE=0; BUNDLED_REASON="Qt's prebuilt binaries need glibc and this system uses musl"
elif [[ "${OS}" == Linux && "${ARCH}" != x86_64 && "${ARCH}" != aarch64 ]]; then
    BUNDLED_POSSIBLE=0; BUNDLED_REASON="Qt publishes no prebuilt binaries for ${ARCH}"
fi

step "System: ${DISTRO_NAME} (${ARCH}), packages via ${MANAGER}"

if [[ "${QT_SOURCE}" == auto ]]; then
    if [[ ${BUNDLED_POSSIBLE} -eq 0 ]]; then
        QT_SOURCE=system
        note "Using the system Qt: ${BUNDLED_REASON}."
    else
        # Refresh the package index first, so the query sees current versions.
        if [[ ${DRY_RUN} -eq 0 ]]; then
            need_root
            case "${MANAGER}" in
                apt) ${SUDO[@]+"${SUDO[@]}"} apt-get update -qq || true ;;
                pacman) ${SUDO[@]+"${SUDO[@]}"} pacman -Sy --noconfirm >/dev/null || true ;;
                zypper) ${SUDO[@]+"${SUDO[@]}"} zypper --non-interactive --gpg-auto-import-keys --quiet refresh || true ;;
                apk) ${SUDO[@]+"${SUDO[@]}"} apk update -q || true ;;
                xbps) ${SUDO[@]+"${SUDO[@]}"} xbps-install -S >/dev/null || true ;;
            esac
        fi
        available="$("system_qt_version_${MANAGER}" 2>/dev/null || true)"
        if [[ -n "${available}" ]] && version_ge "${available}" "${MIN_QT}"; then
            QT_SOURCE=system
            note "The distribution ships Qt ${available}; using it."
        else
            QT_SOURCE=bundled
            note "The distribution's Qt (${available:-none found}) is older than ${MIN_QT}; installing Qt ${QT_VERSION} into ${QT_DIR}."
        fi
    fi
elif [[ "${QT_SOURCE}" == bundled && ${BUNDLED_POSSIBLE} -eq 0 ]]; then
    die "cannot use a bundled Qt: ${BUNDLED_REASON}. Use --qt system."
fi
[[ "${QT_SOURCE}" == system ]] && QT_FROM_SYSTEM=1

# ---- Install --------------------------------------------------------------------

step "Installing build tools and libraries"
confirm "Install packages with ${MANAGER}?" || die "cancelled"
if [[ "${MANAGER}" == brew ]]; then
    install_macos
else
    need_root
    "install_${MANAGER}"
fi

if [[ ${DRY_RUN} -eq 0 ]]; then
    CMAKE_VERSION="$(cmake --version | sed -n '1s/^cmake version \([0-9][0-9.]*\).*/\1/p')"
    version_ge "${CMAKE_VERSION}" "${MIN_CMAKE}" ||
        die "CMake ${CMAKE_VERSION} is too old (need ${MIN_CMAKE}); 'pip install cmake' or https://cmake.org/download/ has a newer one"
fi

QT_PREFIX=""
if [[ ${QT_FROM_SYSTEM} -eq 1 ]]; then
    step "Checking the system Qt"
    QTPATHS=""
    for candidate in qtpaths6 qtpaths /usr/lib/qt6/bin/qtpaths /usr/lib64/qt6/bin/qtpaths \
                     /usr/lib/qt6/bin/qtpaths6 "$( [[ "${OS}" == Darwin ]] && brew --prefix qt 2>/dev/null)/bin/qtpaths"; do
        if command -v "${candidate}" >/dev/null 2>&1; then QTPATHS="$(command -v "${candidate}")"; break; fi
    done
    if [[ ${DRY_RUN} -eq 0 ]]; then
        [[ -n "${QTPATHS}" ]] || die "Qt was installed but qtpaths was not found"
        SYSTEM_QT_VERSION="$("${QTPATHS}" --qt-query QT_VERSION)"
        version_ge "${SYSTEM_QT_VERSION}" "${MIN_QT}" ||
            die "the system Qt is ${SYSTEM_QT_VERSION}; SpacePenguin needs ${MIN_QT}+. Run with --qt bundled."
        QT_PREFIX="$("${QTPATHS}" --qt-query QT_INSTALL_PREFIX)"
        note "Qt ${SYSTEM_QT_VERSION} at ${QT_PREFIX}"
    fi
else
    case "${OS}-${ARCH}" in
        Linux-x86_64) QT_HOST=linux; QT_ARCH=linux_gcc_64 ;;
        Linux-aarch64) QT_HOST=linux_arm64; QT_ARCH=linux_gcc_arm64 ;;
        Darwin-*) QT_HOST=mac; QT_ARCH=clang_64 ;;
    esac
    # aqtinstall names the directory after the architecture, with exceptions it
    # does not report, so look for whatever it created.
    find_qt_prefix() {
        local config
        for config in "${QT_DIR}/${QT_VERSION}"/*/lib/cmake/Qt6WebEngineWidgets/Qt6WebEngineWidgetsConfig.cmake; do
            [[ -f "${config}" ]] && { (cd "$(dirname "${config}")/../../.." && pwd); return 0; }
        done
        return 1
    }
    if QT_PREFIX="$(find_qt_prefix)"; then
        step "Qt ${QT_VERSION} with QtWebEngine is already in ${QT_PREFIX}"
    else
        step "Installing Qt ${QT_VERSION} (${QT_ARCH}) with QtWebEngine into ${QT_DIR}"
        note "This downloads about 1 GB from the Qt mirrors."
        VENV="${QT_DIR}/.venv"
        run python3 -m venv "${VENV}"
        run "${VENV}/bin/python" -m pip install --quiet --upgrade pip aqtinstall
        run "${VENV}/bin/aqt" install-qt "${QT_HOST}" desktop "${QT_VERSION}" "${QT_ARCH}" \
            --outputdir "${QT_DIR}" --modules "${QT_MODULES[@]}"
        if [[ ${DRY_RUN} -eq 0 ]]; then
            QT_PREFIX="$(find_qt_prefix)" || die "aqtinstall finished but no Qt was found in ${QT_DIR}"
        fi
    fi
fi

# CI: put Qt's tools on PATH for the following steps.
if [[ -n "${GITHUB_PATH:-}" && -n "${QT_PREFIX}" ]]; then
    echo "${QT_PREFIX}/bin" >> "${GITHUB_PATH}"
fi

step "Ready"
if [[ ${QT_FROM_SYSTEM} -eq 0 && "${QT_DIR}" != "${ROOT}/.qt" ]]; then
    note "Qt is outside .qt/, so tell CMake where it is:"
    note "    cmake -B build -DCMAKE_PREFIX_PATH=${QT_PREFIX:-${QT_DIR}/${QT_VERSION}/<arch>}"
else
    note "CMake finds Qt automatically. Build and test with:"
    note "    cmake -B build"
fi
note "    cmake --build build --parallel $("${ROOT}/tools/build-jobs.sh")   # jobs this machine has memory for"
note "    QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure"
