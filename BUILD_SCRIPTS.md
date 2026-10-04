# Build scripts for SpacePenguin

## Linux AppImage
```bash
./build-appimage.sh 0.1.0
```
Requires: `linuxdeployqt`, `appimagetool`, Qt6 development packages

## macOS DMG
```bash
./build-dmg.sh 0.1.0
```
Must run on macOS. Requires: Xcode command line tools

## Windows Installer (NSIS)
```bash
makensis installer.nsi
```
Requires: NSIS 3.x, Qt6 Windows binaries

## GitHub Actions Workflow
Create `.github/workflows/build.yml`:

```yaml
name: Build and Release

on:
  push:
    tags:
      - 'v*'
  workflow_dispatch:

jobs:
  linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y qt6-base-dev qt6-webengine-dev cmake ninja-build \
            linuxdeployqt appimagetool
      
      - name: Build
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja
          cmake --build build --config Release
      
      - name: Build AppImage
        run: |
          chmod +x build-appimage.sh
          ./build-appimage.sh ${{ github.ref_name }}
      
      - name: Upload AppImage
        uses: actions/upload-artifact@v4
        with:
          name: SpacePenguin-AppImage
          path: SpacePenguin-*-x86_64.AppImage

  macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Install Qt
        run: |
          brew install qt6 cmake ninja
      
      - name: Build
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja
          cmake --build build --config Release
      
      - name: Build DMG
        run: |
          chmod +x build-dmg.sh
          ./build-dmg.sh ${{ github.ref_name }}
      
      - name: Upload DMG
        uses: actions/upload-artifact@v4
        with:
          name: SpacePenguin-DMG
          path: SpacePenguin-*.dmg

  windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Install Qt
        run: |
          choco install qt6 --version=6.6.0
          choco install ninja cmake nsis
      
      - name: Build
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja
          cmake --build build --config Release
      
      - name: Build Installer
        run: |
          makensis installer.nsi
      
      - name: Upload Installer
        uses: actions/upload-artifact@v4
        with:
          name: SpacePenguin-Installer
          path: SpacePenguin-*-windows-x64.exe

  release:
    needs: [linux, macos, windows]
    runs-on: ubuntu-latest
    if: startsWith(github.ref, 'refs/tags/v')
    steps:
      - name: Download artifacts
        uses: actions/download-artifact@v4
      
      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            SpacePenguin-AppImage/*
            SpacePenguin-DMG/*
            SpacePenguin-Installer/*
          draft: false
          prerelease: false
```