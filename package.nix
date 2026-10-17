# The SpacePenguin derivation, shared by flake.nix, default.nix and shell.nix.
{
  lib,
  stdenv,
  cmake,
  ninja,
  qt6,
}:

let
  # The version lives in one place: the project() call in CMakeLists.txt.
  versionLine = lib.findFirst (line: builtins.match " *VERSION [0-9.]+" line != null) null (
    lib.splitString "\n" (builtins.readFile ./CMakeLists.txt)
  );
  version = builtins.head (builtins.match " *VERSION ([0-9.]+)" versionLine);
in
stdenv.mkDerivation {
  pname = "spacepenguin";
  inherit version;

  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.unions [
      ./CMakeLists.txt
      ./LICENSE
      ./cmake
      ./filters
      ./html
      ./packaging
      ./src
      ./tests
    ];
  };

  nativeBuildInputs = [
    cmake
    ninja
    qt6.wrapQtAppsHook
  ];

  buildInputs = [
    qt6.qtbase
    qt6.qtwebengine
  ];

  cmakeFlags = [
    # Nix provides Qt itself: no qt.conf, no .qt/, no windeployqt/macdeployqt.
    (lib.cmakeBool "SPACEPENGUIN_INSTALL_QT_CONF" false)
    (lib.cmakeBool "SPACEPENGUIN_USE_LOCAL_QT" false)
    (lib.cmakeBool "SPACEPENGUIN_DEPLOY_QT" false)
    (lib.cmakeBool "SPACEPENGUIN_INSTALL_APPARMOR_PROFILE" false)
  ];

  doCheck = true;
  preCheck = ''
    export QT_QPA_PLATFORM=offscreen
    export HOME="$TMPDIR"
  '';

  # On macOS the build is an app bundle; put it where Nix expects apps and add a
  # command-line entry point.
  postInstall = lib.optionalString stdenv.hostPlatform.isDarwin ''
    mkdir -p "$out/Applications" "$out/bin"
    mv "$out/SpacePenguin.app" "$out/Applications/"
    ln -s "$out/Applications/SpacePenguin.app/Contents/MacOS/SpacePenguin" "$out/bin/SpacePenguin"
  '';

  meta = {
    description = "A secure, lightweight, and usable browser written in Qt";
    longDescription = ''
      SpacePenguin is a small web browser built on Qt 6 and QtWebEngine. It blocks
      ads with Adblock Plus style filter lists, runs JavaScript user scripts, keeps
      private windows off the disk, and never phones home.
    '';
    homepage = "https://github.com/developer1-vd/SpacePenguin";
    license = lib.licenses.gpl3Plus;
    mainProgram = "SpacePenguin";
    platforms = lib.platforms.linux ++ lib.platforms.darwin;
  };
}
