#!/usr/bin/env bash
# Package SpacePenguin for the platform this runs on: ./build-all.sh [version]
#
#   Linux   -> SpacePenguin-<version>-<arch>.AppImage   (build-appimage.sh)
#   macOS   -> SpacePenguin-<version>-macos.dmg         (build-dmg.sh)
#   Windows -> installer and zip                        (build-windows.ps1, via Git Bash/MSYS)
#
# Each platform's package has to be built on that platform; the CI workflow in
# .github/workflows/build.yml builds all three.
set -euo pipefail

VERSION="${1:-0.1.0}"

case "$(uname -s)" in
    Linux)
        exec ./build-appimage.sh "${VERSION}"
        ;;
    Darwin)
        exec ./build-dmg.sh "${VERSION}"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        exec powershell -ExecutionPolicy Bypass -File build-windows.ps1 -Version "${VERSION}"
        ;;
    *)
        echo "Unsupported platform: $(uname -s)" >&2
        exit 1
        ;;
esac
