# Build scripts for SpacePenguin

| Script | Runs on | Makes |
| ------ | ------- | ----- |
| `install-deps.sh` | Linux, macOS | a machine ready to build: toolchain, libraries, Qt 6.8 with QtWebEngine |
| `install-deps.ps1` | Windows | the same for Windows: Visual Studio Build Tools, CMake, Ninja, NSIS, Qt for MSVC |
| `build-appimage.sh` | Linux | `dist/SpacePenguin-<version>-<arch>.AppImage` |
| `build-linux-packages.sh` | Linux | `dist/*.deb` and/or `dist/*.rpm` for the distribution it runs on |
| `packaging/arch/PKGBUILD` | Arch and derivatives | `spacepenguin-<version>-1-<arch>.pkg.tar.zst` |
| `packaging/flatpak/…yml` | any Linux with flatpak-builder | `SpacePenguin.flatpak` |
| `flake.nix` / `default.nix` | NixOS, any Linux or macOS with Nix | the browser in the Nix store (`./result`) |
| `build-dmg.sh` | macOS | `dist/SpacePenguin-<version>-macos.dmg` and `.zip` of the `.app` |
| `build-windows.ps1` | Windows | `dist/SpacePenguin-<version>-windows-x64-setup.exe` and `.zip` |
| `build-android.sh` | Linux, macOS | `dist/SpacePenguin-<version>-android-arm64.apk` (the phone app in `mobile/`) |
| `build-ios.sh` | macOS with Xcode | an iOS Simulator build, or a signed `.ipa` with `--team` |
| `build-all.sh` | any | whichever of the above this machine can make |
| `install.sh` | any Linux, ChromeOS Linux | the AppImage installed per user, with an app-menu entry (`packaging/chromeos/install.sh` runs it) |
| `tools/build-jobs.sh` | Linux, macOS | the number of compiler jobs this machine has memory for |

Every packaging script reads the version from `CMakeLists.txt` unless given one,
finds Qt in `.qt/` (from `install-deps`) before `CMAKE_PREFIX_PATH`, launches the
finished app with `--version` to check that it starts, and writes a `.sha256`
checksum next to the package. All of them take `--help` (`-Help` on Windows).

## install-deps.sh

```bash
./install-deps.sh               # everything, asking before it installs packages
./install-deps.sh --yes         # no questions (CI)
./install-deps.sh --dry-run     # show the commands only
./install-deps.sh --qt system   # use the distribution's Qt (must be 6.8+)
./install-deps.sh --qt bundled  # always use Qt from the Qt mirrors, in .qt/
```

1. Identifies the system from `/etc/os-release` (or macOS) and picks its package
   manager: apt, dnf/yum, pacman, zypper, apk, xbps, emerge or Homebrew. On RHEL,
   Rocky and AlmaLinux it enables CodeReady Builder and EPEL first. NixOS is
   pointed at `shell.nix`.
2. With `--qt auto` (the default), asks the package manager which QtWebEngine it
   offers. 6.8 or newer: the system Qt is installed and used. Older, or none: Qt
   6.8.3 comes from the Qt mirrors instead. musl systems (Alpine, Void musl) and
   CPUs Qt does not build for always use the system Qt.
3. Installs the compiler, CMake, Ninja, Git, Python, the OpenGL, xkbcommon,
   xcb-util, NSS, ALSA and fontconfig libraries QtWebEngine needs, and the
   packaging checkers (`desktop-file-utils`, `appstream`, `dpkg-dev`/`rpm-build`).
4. For the bundled Qt: creates a Python virtual environment in `.qt/.venv`,
   installs `aqtinstall` there, and installs Qt with the QtWebEngine,
   QtWebChannel, QtPositioning, QtPdf and QtSerialPort modules into `.qt/`.
5. Checks CMake is 3.21+ and the Qt is 6.8+, and prints the build commands.

The script needs bash: on Alpine run `apk add bash`, on Void `xbps-install bash`.

## build-android.sh and build-ios.sh

```bash
./build-android.sh [--release]          # QT_ANDROID_KEYSTORE_* for --release
./build-ios.sh [--team <id> [--export-method development|ad-hoc|app-store]]
```

They build the phone app in `mobile/` (Qt Quick and QtWebView; see the README's
*Phones* section for what it can and cannot do). Each fetches its Qt with
aqtinstall into `.qt/` — the phone kit with QtWebView, and the desktop kit its
build tools run from. `build-android.sh` also fetches the Android command-line
tools (checked against their SHA-256), then the SDK platform, build tools and
NDK 26.1 that Qt 6.8 is built with, into `.cache/android-sdk` unless
`ANDROID_SDK_ROOT` points at an SDK; it needs a JDK 17 or newer. Without
`--release` the APK is signed with a debug key, which installs fine but is not
accepted by Google Play. `build-ios.sh` generates an Xcode project; without
`--team` it builds an unsigned Simulator app, with it an archive exported as an
`.ipa` signed through your Xcode account.

## Build parallelism

Compiling one SpacePenguin source file takes about 500 MB. Every script builds
with `--parallel "$(tools/build-jobs.sh)"` — one job per GB of free memory, at
most one per CPU — and `CMakeLists.txt` sets Ninja job pools to the same number
(`SPACEPENGUIN_BUILD_JOBS` overrides both). An unlimited `make -j` can otherwise
start a job per file and freeze a laptop.

## install-deps.ps1

```powershell
powershell -ExecutionPolicy Bypass -File install-deps.ps1 [-QtVersion 6.8.3] [-SkipVisualStudio] [-NoPackaging] [-DryRun]
```

Uses winget, or Chocolatey where winget is missing (Windows Server). Installs Visual
Studio 2022 Build Tools with the C++ workload when `vswhere` finds no MSVC, then
CMake, Ninja, Python 3 and NSIS when they are missing, then Qt for MSVC 2022 into
`.qt\` through `aqtinstall`. Open a new terminal afterwards so the new tools are on
`PATH`.

## build-appimage.sh

```bash
./build-appimage.sh [--version x.y.z] [--qt <prefix>] [--update-info <info>] [--sign] [--skip-build]
```

1. Builds with `CMAKE_INSTALL_PREFIX=/usr` and installs into an AppDir, which
   brings the desktop entry, AppStream metadata and icons.
2. Downloads `linuxdeploy` and `linuxdeploy-plugin-qt` into `.cache/linuxdeploy`
   the first time.
3. Bundles Qt and QtWebEngine (helper process, resources, locales) and the
   libraries they need, with the X11, native Wayland and offscreen platform
   plugins, and writes the AppImage. `--update-info` embeds AppImageUpdate data
   and writes a `.zsync`; `--sign` signs with your GPG key.
4. Launches the AppImage with a scrubbed environment to check it starts.

An AppImage runs on distributions whose glibc is at least as new as the build
machine's, which is why CI builds it on Ubuntu 22.04. The ARM64 AppImage is built
on Ubuntu 24.04, because Qt's ARM64 Linux binaries need glibc 2.38; it runs on
Ubuntu 24.04, Debian 13, Fedora 39 and newer.

## install.sh

```bash
curl -fsSL https://raw.githubusercontent.com/developer1-vd/SpacePenguin/main/install.sh | bash
bash install.sh [--yes] [--no-deps] [SpacePenguin-<version>-<arch>.AppImage]
bash install.sh --uninstall [--purge]      # or: uninstall.sh, the same, also as a curl one-liner
```

The Linux installer, for any distribution and for Chromebooks
(`packaging/chromeos/install.sh` runs it): installs the libraries the AppImage
takes from the system with apt, dnf, zypper or pacman, downloads the newest
release's AppImage for this CPU — or, before there is a release, the *nightly*
pre-release that CI publishes from `main` — (checking its `.sha256`) unless given one,
unpacks it into `~/.local/opt/spacepenguin`, links `~/.local/bin/spacepenguin`,
installs the launcher entry and icon — which ChromeOS copies into its own
launcher — and checks it starts. CI runs it in stock Debian containers, the
base of ChromeOS's Linux environment, and in Fedora, openSUSE Tumbleweed and Arch.

## build-linux-packages.sh

```bash
./build-linux-packages.sh [deb|rpm|tgz|auto]...
```

Builds against the distribution's own Qt (it refuses a Qt from `.qt/`, which a
package cannot depend on), installs to `/usr`, and runs CPack. The `.deb` gets its
dependencies from `dpkg-shlibdeps` plus the Qt platform plugins and the
QtWebEngine helper; the `.rpm` from RPM's automatic dependency scan. The
distribution must ship Qt 6.8+: Debian 13, Ubuntu 25.04, Fedora 41, openSUSE
Tumbleweed and newer. Elsewhere use the AppImage or the Flatpak.

## packaging/arch/PKGBUILD

```bash
cd packaging/arch && makepkg -si                       # from the v<version> release tarball
SPACEPENGUIN_SOURCE="$(git rev-parse --show-toplevel)" makepkg -si   # from this checkout
```

Builds against Arch's `qt6-webengine`, runs the tests in `check()`, and installs
the license as Arch requires.

## packaging/flatpak

```bash
flatpak install --user flathub org.kde.Platform//6.8 org.kde.Sdk//6.8 io.qt.qtwebengine.BaseApp//6.8
flatpak-builder --user --install --force-clean build-flatpak packaging/flatpak/org.spacepenguin.SpacePenguin.yml
flatpak run org.spacepenguin.SpacePenguin
```

Built on the KDE 6.8 runtime with the QtWebEngine base app. The app may use the
network, the display (Wayland, or X11 as a fallback), sound, the GPU and the
Downloads folder, and nothing else.

## Nix: flake.nix, package.nix, default.nix, shell.nix

```bash
nix build          # or nix-build
nix run            # build and start it
nix develop        # or nix-shell: the toolchain and Qt, for cmake -B build
nix flake check    # build and run the tests
```

`package.nix` is the derivation: CMake and Ninja with `wrapQtAppsHook`, Qt and
QtWebEngine from nixpkgs, the tests run in `checkPhase` with the offscreen
platform, and the version read from `CMakeLists.txt`. It turns off the qt.conf,
`.qt/` lookup and Qt deployment, which Nix does not need. On macOS the app bundle
goes to `$out/Applications` with a `bin/SpacePenguin` link.

`flake.nix` exposes it as `packages.default`, `apps.default`, `checks`, an
overlay (`pkgs.spacepenguin`) and a NixOS module (`programs.spacepenguin.enable`,
with `defaultBrowser` to register it for web links). Its nixpkgs input is the
NixOS channel tarball rather than `github:`, which needs no GitHub API calls and
so is not rate limited. `shell.nix` is the development shell; it also sets
`QT_PLUGIN_PATH` and `QTWEBENGINEPROCESS_PATH` so a binary run straight from the
build tree finds Qt.

## build-dmg.sh

```bash
./build-dmg.sh [--universal | --arch arm64] [--sign "Developer ID Application: …" [--notarize]] [--app-only]
```

1. Builds `SpacePenguin.app` (universal with `--universal`; the Qt from
   install-deps is universal) and installs it, which runs `macdeployqt` to copy
   the Qt frameworks and QtWebEngine's helper app into the bundle.
2. Signs from the inside out: libraries and plugins, the nested
   `QtWebEngineProcess.app`, the frameworks, then the app. With a Developer ID it
   uses the hardened runtime and `packaging/macos/entitlements.plist` (QtWebEngine
   needs JIT); without one it signs ad-hoc. Then verifies the signature.
3. Launches the app to check it starts.
4. Zips the `.app` with `ditto` and makes a compressed DMG with an
   `/Applications` link, then verifies it.
5. `--notarize` submits the DMG with `notarytool` (credentials from
   `NOTARY_PROFILE`, or `APPLE_ID`, `APPLE_TEAM_ID` and `APPLE_APP_PASSWORD`),
   staples the ticket and checks it with Gatekeeper.

## build-windows.ps1

```powershell
powershell -ExecutionPolicy Bypass -File build-windows.ps1 [-Version x.y.z] [-CertificateThumbprint <hash>] [-NoInstaller] [-NoZip]
```

1. Builds with MSVC and installs into `build-windows\dist`; the install step runs
   `windeployqt`.
2. Copies the MSVC runtime DLLs beside the executable (app-local deployment), so
   the app runs on a clean Windows without the VC++ redistributable.
3. Checks every file the browser needs is present, then launches it with
   `--version`.
4. Signs the executable with `signtool` when given a certificate.
5. Zips the folder as a portable copy and builds the installer from
   `installer.nsi`, then signs the installer.

### installer.nsi

- Install for the current user (no administrator rights, into
  `%LOCALAPPDATA%\Programs`) or for everyone (into Program Files), chosen on a page or
  with `/CurrentUser` / `/AllUsers`; `/S` installs silently.
- Upgrades in place: an existing installation is removed first, keeping the
  user's browsing data.
- Start menu entries, including one for a private window; optional desktop
  shortcut; optional registration in Settings › Apps › Default apps (Windows asks
  the user before it becomes the default — nothing is taken over silently).
- An Apps & features entry with icon, version, size and a quiet uninstall string.
- The uninstaller removes the program and its registration, and only deletes
  history, bookmarks, settings and cache when "Remove browsing data" is ticked.
- Always installs into a folder of its own, so the uninstaller's recursive
  delete can never reach anything else.

## Continuous integration

`.github/workflows/build.yml`, on every push and pull request:

- **Linux AppImage (x86-64 and ARM64), Windows, macOS** — `install-deps`, build,
  all tests, the packages above, the AppImage installed on Debian 12 with the
  Chromebook script, and checks that launch them: the AppImage, both architectures
  of the macOS app from inside the mounted DMG, and on Windows a silent per-user
  install, launch and silent uninstall.
- **Distributions** (in containers) — Debian 13, Ubuntu 24.04, Fedora,
  openSUSE Tumbleweed, Arch, Alpine and Void each run
  `.github/scripts/distro.sh`: `install-deps`, build, tests, then the native
  package (`.deb`, `.rpm`, Arch package) installed with the distribution's own
  package manager and launched.
- **Nix** — `nix flake check`, `nix build` with a launch check, `nix-build`, and a
  build from source in `nix develop`, on Linux and macOS.
- **Flatpak** — built with `flatpak-builder` into a bundle.

A tag such as `v0.1.0` also publishes every package as a GitHub release.
