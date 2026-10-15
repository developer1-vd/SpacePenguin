# Development shell for NixOS and other Nix users:
#
#   nix-shell --run 'cmake -B build && cmake --build build --parallel'
#
# Provides the toolchain and Qt 6 with QtWebEngine from nixpkgs (Qt 6.8 or newer
# is in nixpkgs 24.11 and later).
{ pkgs ? import <nixpkgs> { } }:

pkgs.mkShell {
  packages = with pkgs; [
    cmake
    ninja
    git
    qt6.qtbase
    qt6.qtwebengine
    qt6.qttools
    qt6.wrapQtAppsHook
    desktop-file-utils
    appstream
  ];

  # Lets the browser find Qt's plugins and QtWebEngineProcess when run from the
  # build tree inside the shell.
  shellHook = ''
    export QT_PLUGIN_PATH="${pkgs.qt6.qtbase}/${pkgs.qt6.qtbase.qtPluginPrefix}:${pkgs.qt6.qtwebengine}/${pkgs.qt6.qtbase.qtPluginPrefix}"
    export QTWEBENGINEPROCESS_PATH="${pkgs.qt6.qtwebengine}/libexec/QtWebEngineProcess"
  '';
}
