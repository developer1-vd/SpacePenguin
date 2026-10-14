# Build a Windows installer and a portable zip:
#   powershell -ExecutionPolicy Bypass -File build-windows.ps1 [-Version 0.1.0]
#
# Run from a "Developer PowerShell for VS" (or any shell with MSVC on PATH) with Qt's
# bin directory on PATH or CMAKE_PREFIX_PATH set. `cmake --install` runs
# windeployqt, so the install directory is a complete, portable copy of the app.
# The installer is built when NSIS (makensis) is available.
param(
    [string]$Version = "0.1.0",
    [string])
$ErrorActionPreference = "Stop"

$DistDir = Join-Path $BuildDir "dist"

Write-Host "==> Building SpacePenguin $Version"
cmake -S . -B $BuildDir -DSPACEPENGUIN_BUILD_TESTS=OFF
if ($LASTEXITCODE) { exit $LASTEXITCODE }
cmake --build $BuildDir --config Release --parallel
if ($LASTEXITCODE) { exit $LASTEXITCODE }

if (Test-Path $DistDir) { Remove-Item -Recurse -Force $DistDir }
cmake --install $BuildDir --config Release --prefix $DistDir
if ($LASTEXITCODE) { exit $LASTEXITCODE }

$Zip = "SpacePenguin-$Version-windows-x64.zip"
Write-Host "==> Creating $Zip"
if (Test-Path $Zip) { Remove-Item $Zip }
Compress-Archive -Path (Join-Path $DistDir "*") -DestinationPath $Zip

$makensis = Get-Command makensis -ErrorAction SilentlyContinue
if ($makensis) {
    Write-Host "==> Creating installer"
    & $makensis.Source "/DVERSION=$Version" "/DSOURCE_DIR=$((Resolve-Path $DistDir).Path)" installer.nsi
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
} else {
    Write-Host "makensis not found; skipping the installer (the zip is ready)."
}
