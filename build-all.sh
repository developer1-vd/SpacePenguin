#!/bin/bash
# Build all packages for SpacePenguin
set -euo pipefail

VERSION="${1:-0.1.0}"
PLATFORM="${2:-all}"

echo "Building SpacePenguin v${VERSION} for ${PLATFORM}..."

# Build the project first
echo "Building project..."
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

case "${PLATFORM}" in
    linux|all)
        echo "Building Linux AppImage..."
        chmod +x build-appimage.sh
        ./build-appimage.sh "${VERSION}"
        ;;
esac

case "${PLATFORM}" in
    macos|all)
        if [[ "$(uname)" == "Darwin" ]]; then
            echo "Building macOS DMG..."
            chmod +x build-dmg.sh
            ./build-dmg.sh "${VERSION}"
        else
            echo "Skipping macOS build (not on macOS)"
        fi
        ;;
esac

case "${PLATFORM}" in
    windows|all)
        if command -v makensis &> /dev/null; then
            echo "Building Windows installer..."
            makensis installer.nsi
        else
            echo "Skipping Windows build (NSIS not found)"
        fi
        ;;
esac

echo "Build complete!"
ls -la SpacePenguin-* 2>/dev/null || true