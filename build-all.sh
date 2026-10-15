#!/usr/bin/env bash
# Build every package this machine can make: ./build-all.sh [options passed on]
#
#   Linux   -> AppImage (build-appimage.sh), plus .deb/.rpm when the distribution's
#              own Qt is 6.8+ (build-linux-packages.sh)
#   macOS   -> universal .app zip and DMG (build-dmg.sh --universal)
#   Windows -> installer and portable zip (build-windows.ps1, from Git Bash/MSYS)
#
# Packages for the other systems have to be built on them; CI
# (.github/workflows/build.yml) builds all of them.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

case "$(uname -s)" in
    Linux)
        ./build-appimage.sh "$@"
        qtpaths="$(command -v qtpaths6 || command -v qtpaths || true)"
        if [[ -n "${qtpaths}" ]] &&
           [[ "$(printf '%s\n6.8.0\n' "$("${qtpaths}" --qt-query QT_VERSION)" | sort -V | head -n1)" == 6.8.0 ]] &&
           { command -v dpkg-deb >/dev/null || command -v rpmbuild >/dev/null; }; then
            ./build-linux-packages.sh
        else
            echo "Skipping .deb/.rpm: they need the distribution's own Qt 6.8+ (./install-deps.sh --qt system)."
        fi
        ;;
    Darwin)
        exec ./build-dmg.sh --universal "$@"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        exec powershell -ExecutionPolicy Bypass -File build-windows.ps1 "$@"
        ;;
    *)
        echo "Unsupported platform: $(uname -s)" >&2
        exit 1
        ;;
esac
