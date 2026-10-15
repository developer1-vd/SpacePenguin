#!/usr/bin/env bash
# CI check for one Linux distribution, run as root inside its container with the
# repository at /src:
#
#   docker run --rm -v "$PWD:/src" -w /src <image> /src/.github/scripts/distro.sh <package>
#
# Installs the dependencies with install-deps.sh, builds, runs the tests, then
# builds the native package (deb, rpm or pkg; "none" to skip), installs it with the
# distribution's package manager and launches the installed browser.
set -euo pipefail

PACKAGE="${1:-none}"
export LC_ALL=C.UTF-8 QT_QPA_PLATFORM=offscreen

step() { printf '\n==> %s\n' "$*"; }

step "Dependencies"
./install-deps.sh --yes

step "Build and test"
rm -rf build-ci
cmake -S . -B build-ci -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-ci --parallel
ctest --test-dir build-ci --output-on-failure

launch() {
    step "Launching the installed browser"
    local reported
    reported="$(HOME="$(mktemp -d)" "$1" --version 2>&1 | tail -n1)"
    echo "    ${reported}"
    [[ "${reported}" == SpacePenguin\ * ]] || { echo "error: it did not start" >&2; exit 1; }
}

case "${PACKAGE}" in
    deb)
        ./build-linux-packages.sh deb
        step "Installing the .deb"
        apt-get install -y ./dist/spacepenguin_*.deb
        launch /usr/bin/SpacePenguin
        ;;
    rpm)
        ./build-linux-packages.sh rpm
        step "Installing the .rpm"
        if command -v dnf >/dev/null; then
            dnf install -y ./dist/spacepenguin-*.rpm
        else
            zypper --non-interactive install --allow-unsigned-rpm ./dist/spacepenguin-*.rpm
        fi
        launch /usr/bin/SpacePenguin
        ;;
    pkg)
        step "Building the Arch package with makepkg"
        # makepkg refuses to run as root.
        useradd -m builder
        git config --system --add safe.directory '*'
        mkdir -p /tmp/makepkg && cp packaging/arch/PKGBUILD /tmp/makepkg/
        chown -R builder /tmp/makepkg
        su builder -c "cd /tmp/makepkg && SPACEPENGUIN_SOURCE=/src makepkg --nodeps --noconfirm"
        mkdir -p dist && cp /tmp/makepkg/spacepenguin-*.pkg.tar.zst dist/
        step "Installing the package"
        pacman -U --noconfirm dist/spacepenguin-*.pkg.tar.zst
        launch /usr/bin/SpacePenguin
        ;;
    none) ;;
    *) echo "unknown package type ${PACKAGE}" >&2; exit 2 ;;
esac
