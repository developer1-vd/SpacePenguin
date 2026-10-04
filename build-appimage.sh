#!/bin/bash
# Build script for Linux AppImage
set -euo pipefail

# Configuration
PROJECT_NAME="SpacePenguin"
BUILD_DIR="build"
APPDIR="${BUILD_DIR}/${PROJECT_NAME}.AppDir"
VERSION="${1:-0.1.0}"

echo "Building ${PROJECT_NAME} AppImage v${VERSION}..."

# Clean and create AppDir
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/lib"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
mkdir -p "${APPDIR}/usr/share/metainfo"

# Copy binary
cp "${BUILD_DIR}/${PROJECT_NAME}" "${APPDIR}/usr/bin/"

# Copy Qt libraries and plugins using linuxdeployqt (if available) or manual copy
if command -v linuxdeployqt &> /dev/null; then
    linuxdeployqt "${APPDIR}/usr/bin/${PROJECT_NAME}" -appimage
else
    # Manual Qt deployment - copy required libraries
    echo "linuxdeployqt not found, copying Qt libraries manually..."
    
    # Find Qt installation
    QT_PATH=$(dirname $(dirname $(which qmake6 2>/dev/null || which qmake 2>/dev/null)) 2>/dev/null || echo "")
    if [ -z "${QT_PATH}" ]; then
        QT_PATH="/usr/lib/qt6"
    fi
    
    # Copy Qt libraries
    cp -r "${QT_PATH}/lib/libQt6Core.so."* "${APPDIR}/usr/lib/" 2>/dev/null || true
    cp -r "${QT_PATH}/lib/libQt6Gui.so."* "${APPDIR}/usr/lib/" 2>/dev/null || true
    cp -r "${QT_PATH}/lib/libQt6Widgets.so."* "${APPDIR}/usr/lib/" 2>/dev/null || true
    cp -r "${QT_PATH}/lib/libQt6WebEngineCore.so."* "${APPDIR}/usr/lib/" 2>/dev/null || true
    cp -r "${QT_PATH}/lib/libQt6WebEngineWidgets.so."* "${APPDIR}/usr/lib/" 2>/dev/null || true
    cp -r "${QT_PATH}/lib/libQt6Network.so."* "${APPDIR}/usr/lib/" 2>/dev/null || true
    cp -r "${QT_PATH}/lib/libQt6DBus.so."* "${APPDIR}/usr/lib/" 2>/dev/null || true
    cp -r "${QT_PATH}/lib/libQt6OpenGL.so."* "${APPDIR}/usr/lib/" 2>/dev/null || true
    cp -r "${QT_PATH}/lib/libQt6PrintSupport.so."* "${APPDIR}/usr/lib/" 2>/dev/null || true
    
    # Copy WebEngine resources
    mkdir -p "${APPDIR}/usr/lib/qt6/resources"
    cp -r "${QT_PATH}/resources/qtwebengine_resources"* "${APPDIR}/usr/lib/qt6/resources/" 2>/dev/null || true
    cp -r "${QT_PATH}/resources/icudtl.dat" "${APPDIR}/usr/lib/qt6/resources/" 2>/dev/null || true
    
    # Copy WebEngine process
    cp "${QT_PATH}/libexec/QtWebEngineProcess" "${APPDIR}/usr/bin/" 2>/dev/null || true
    
    # Copy Qt plugins
    mkdir -p "${APPDIR}/usr/plugins"
    cp -r "${QT_PATH}/plugins/platforms" "${APPDIR}/usr/plugins/" 2>/dev/null || true
    cp -r "${QT_PATH}/plugins/webengine" "${APPDIR}/usr/plugins/" 2>/dev/null || true
    cp -r "${QT_PATH}/plugins/iconengines" "${APPDIR}/usr/plugins/" 2>/dev/null || true
    cp -r "${QT_PATH}/plugins/imageformats" "${APPDIR}/usr/plugins/" 2>/dev/null || true
fi

# Create desktop file
cat > "${APPDIR}/usr/share/applications/${PROJECT_NAME}.desktop" << EOF
[Desktop Entry]
Name=SpacePenguin
Comment=A secure, lightweight, and usable browser written in Qt
Exec=${PROJECT_NAME} %U
Icon=${PROJECT_NAME}
Terminal=false
Type=Application
Categories=Network;WebBrowser;
MimeType=text/html;text/xml;application/xhtml+xml;application/xml;application/vnd.mozilla.xul+xml;application/rss+xml;application/rdf+xml;x-scheme-handler/http;x-scheme-handler/https;
StartupNotify=true
EOF

# Create AppStream metadata
cat > "${APPDIR}/usr/share/metainfo/${PROJECT_NAME}.metainfo.xml" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop">
  <id>${PROJECT_NAME}.desktop</id>
  <name>SpacePenguin</name>
  <summary>A secure, lightweight, and usable browser written in Qt</summary>
  <description>
    <p>SpacePenguin is a browser focused on privacy, security, and usability. Built with Qt 6 and QtWebEngine.</p>
    <p>Features: ad blocking, user scripts, theme support, private browsing, downloads, bookmarks, history.</p>
  </description>
  <url type="homepage">https://github.com/yourusername/SpacePenguin</url>
  <url type="bugtracker">https://github.com/yourusername/SpacePenguin/issues</url>
  <license>GPL-3.0-or-later</license>
  <releases>
    <release version="${VERSION}" date="$(date +%Y-%m-%d)"/>
  </releases>
</component>
EOF

# Copy icon (use a placeholder if not available)
if [ -f "assets/icon.png" ]; then
    cp assets/icon.png "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${PROJECT_NAME}.png"
elif [ -f "html/icon.png" ]; then
    cp html/icon.png "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${PROJECT_NAME}.png"
else
    # Create a simple placeholder icon
    convert -size 256x256 xc:transparent -fill "#2f6fed" -draw "circle 128,128 128,20" \
        -fill white -font DejaVu-Sans-Bold -pointsize 100 -gravity center -annotate +0+10 "🐧" \
        "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${PROJECT_NAME}.png" 2>/dev/null || \
    echo "Could not create icon, using placeholder"
fi

# Create AppRun script
cat > "${APPDIR}/AppRun" << 'EOF'
#!/bin/bash
set -euo pipefail

HERE="$(dirname "$(readlink -f "${0}")")"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins:${QT_PLUGIN_PATH:-}"
export QML2_IMPORT_PATH="${HERE}/usr/qml:${QML2_IMPORT_PATH:-}"

# QtWebEngine specific
export QTWEBENGINEPROCESS_PATH="${HERE}/usr/bin/QtWebEngineProcess"
export QTWEBENGINE_RESOURCES_PATH="${HERE}/usr/lib/qt6/resources"

exec "${HERE}/usr/bin/SpacePenguin" "$@"
EOF
chmod +x "${APPDIR}/AppRun"

# Build AppImage
if command -v appimagetool &> /dev/null; then
    appimagetool "${APPDIR}" "${PROJECT_NAME}-${VERSION}-x86_64.AppImage"
    echo "AppImage created: ${PROJECT_NAME}-${VERSION}-x86_64.AppImage"
else
    echo "appimagetool not found. Install it to create the AppImage."
    echo "AppDir ready at: ${APPDIR}"
fi