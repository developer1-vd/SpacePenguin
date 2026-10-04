#!/bin/bash
# Build script for macOS DMG installer
set -euo pipefail

# Configuration
PROJECT_NAME="SpacePenguin"
BUILD_DIR="build"
APP_BUNDLE="${BUILD_DIR}/${PROJECT_NAME}.app"
DMG_NAME="${PROJECT_NAME}-${VERSION:-0.1.0}-macos.dmg"
VERSION="${1:-0.1.0}"

echo "Building ${PROJECT_NAME} DMG v${VERSION}..."

# Check if we're on macOS
if [[ "$(uname)" != "Darwin" ]]; then
    echo "This script must be run on macOS"
    exit 1
fi

# Verify app bundle exists
if [ ! -d "${APP_BUNDLE}" ]; then
    echo "App bundle not found at ${APP_BUNDLE}. Build the project first."
    exit 1
fi

# Create a temporary directory for DMG contents
DMG_DIR=$(mktemp -d)
trap "rm -rf ${DMG_DIR}" EXIT

# Copy app bundle
cp -R "${APP_BUNDLE}" "${DMG_DIR}/"

# Create Applications symlink
ln -s /Applications "${DMG_DIR}/Applications"

# Create DMG background (optional)
# You can add a background image at resources/dmg-background.png
if [ -f "resources/dmg-background.png" ]; then
    cp resources/dmg-background.png "${DMG_DIR}/.background.png"
fi

# Calculate required size (in MB)
APP_SIZE=$(du -sm "${APP_BUNDLE}" | cut -f1)
DMG_SIZE=$((APP_SIZE + 100))  # Add 100MB padding

# Create DMG
echo "Creating DMG..."
hdiutil create \
    -volname "${PROJECT_NAME} ${VERSION}" \
    -srcfolder "${DMG_DIR}" \
    -ov \
    -format UDZO \
    -imagekey zlib-level=9 \
    "${DMG_NAME}"

# Sign the DMG (if certificates available)
if security find-identity -v -p codesigning | grep -q "Developer ID Application"; then
    echo "Signing DMG..."
    codesign --force --sign "Developer ID Application" "${DMG_NAME}"
    
    # Notarize (requires Apple Developer account)
    # xcrun notarytool submit "${DMG_NAME}" --apple-id "your@email.com" --team-id "TEAM_ID" --password "password" --wait
    # xcrun stapler staple "${DMG_NAME}"
fi

echo "DMG created: ${DMG_NAME}"
echo "Size: $(du -h "${DMG_NAME}" | cut -f1)"