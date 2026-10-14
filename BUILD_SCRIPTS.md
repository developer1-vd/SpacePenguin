# Build scripts for SpacePenguin

Every package is built on the platform it is for. All three scripts build with
CMake into their own directory (override with `BUILD_DIR=…` / `-BuildDir …`) and
then run `cmake --install`, so they package exactly what a normal install
produces. `.github/workflows/build.yml` runs them on every push.

`./build-all.sh <version>` runs whichever script matches the current machine.

## Linux AppImage — `build-appimage.sh`

```bash
CMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64 ./build-appimage.sh 0.1.0
```

1. Configures with `CMAKE_INSTALL_PREFIX=/usr` and without the `qt.conf` a normal
   Linux install gets (the AppImage carries its own Qt).
2. Installs into `build-appimage/AppDir`, which brings the desktop entry,
   AppStream metadata and icons from `packaging/linux` and `packaging/icons`.
3. Downloads `linuxdeploy` and `linuxdeploy-plugin-qt` into `.cache/linuxdeploy`
   the first time, then bundles Qt, QtWebEngine (helper process, resources,
   locales) and the libraries they need.

Qt is found through `qmake`: the script uses `QMAKE` if set, then
`$CMAKE_PREFIX_PATH/bin/qmake`, then `qmake6`/`qmake` on `PATH`. The Qt install
needs the QtWebEngine, QtWebChannel, QtPositioning and QtSerialPort modules (the
last one because the positioning plugins link against it).

Output: `SpacePenguin-<version>-<arch>.AppImage`.

## macOS DMG — `build-dmg.sh`

```bash
CMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/macos ./build-dmg.sh 0.1.0
```

1. Builds `SpacePenguin.app` (icon, `Info.plist` with the `http`/`https` URL
   types) and installs it into `build-macos/dist`; the install step runs
   `macdeployqt`, which copies the Qt frameworks and QtWebEngine's helper app
   into the bundle.
2. Signs the bundle: with `CODESIGN_IDENTITY="Developer ID Application: …"` it
   uses the hardened runtime and `packaging/macos/entitlements.plist` (QtWebEngine
   needs JIT), ready for `xcrun notarytool`; without it, ad-hoc.
3. Wraps the app and an `/Applications` link in a compressed DMG.

Set `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` with a universal Qt for a universal
app.

Output: `SpacePenguin-<version>-macos.dmg`.

## Windows installer and zip — `build-windows.ps1`

```powershell
# From a "Developer PowerShell for VS 2022", with Qt's bin directory on PATH
powershell -ExecutionPolicy Bypass -File build-windows.ps1 -Version 0.1.0
```

1. Builds with MSVC and installs into `build-windows\dist`; the install step runs
   `windeployqt`, so that directory is a complete portable copy.
2. Zips it as `SpacePenguin-<version>-windows-x64.zip`.
3. If `makensis` (NSIS 3) is on `PATH`, builds
   `SpacePenguin-<version>-windows-x64-setup.exe` from `installer.nsi`.

The installer puts SpacePenguin in its own folder under Program Files, adds Start
menu entries (including a private-window one), and optionally a desktop shortcut
and a "default browser" registration. That registration only lists SpacePenguin
in Settings › Apps › Default apps; Windows asks the user before it becomes the
default. The uninstaller removes all of it.

`installer.nsi` can also be built by hand:
`makensis /DVERSION=0.1.0 /DSOURCE_DIR=<install dir> installer.nsi`.

## Continuous integration

`.github/workflows/build.yml` runs on Linux, Windows and macOS for every push and
pull request: configure, build, run the tests offscreen, package with the script
above, check that the package contains QtWebEngine, and upload it as an artifact.
Pushing a tag such as `v0.1.0` also creates a GitHub release with the three
packages attached.
