# Development shell, for `nix develop` (through flake.nix) or `nix-shell`:
#
#   nix develop        # or: nix-shell
#   cmake -B build -G Ninja && cmake --build build
#   ./build/SpacePenguin
#
# It has everything the package build uses (CMake, Ninja, Qt 6 with QtWebEngine
# from nixpkgs) plus the tools for checking the packaging files.
{
  pkgs ? import <nixpkgs> { },
}:

let
  spacepenguin = pkgs.callPackage ./package.nix { };
  qtPlugins = pkgs.qt6.qtbase.qtPluginPrefix;
in
pkgs.mkShell {
  inputsFrom = [ spacepenguin ];

  packages = with pkgs; [
    git
    gdb
    desktop-file-utils
    appstream
    shellcheck
  ];

  # A binary run straight from the build tree is not wrapped by wrapQtAppsHook,
  # so point it at Qt's plugins and QtWebEngine's helper process.
  shellHook =
    if pkgs.stdenv.hostPlatform.isLinux then
      ''
        export QT_PLUGIN_PATH="${pkgs.qt6.qtbase}/${qtPlugins}:${pkgs.qt6.qtwayland}/${qtPlugins}:${pkgs.qt6.qtwebengine}/${qtPlugins}"
        export QTWEBENGINEPROCESS_PATH="${pkgs.qt6.qtwebengine}/libexec/QtWebEngineProcess"
        echo "SpacePenguin dev shell: cmake -B build -G Ninja && cmake --build build"
      ''
    else
      ''
        export QT_PLUGIN_PATH="${pkgs.qt6.qtbase}/${qtPlugins}"
        echo "SpacePenguin dev shell: cmake -B build -G Ninja && cmake --build build"
      '';
}
