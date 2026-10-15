# Build SpacePenguin for Windows and package it as an installer and a portable zip:
#
#   powershell -ExecutionPolicy Bypass -File build-windows.ps1
#   powershell -ExecutionPolicy Bypass -File build-windows.ps1 -Help
#
# Needs Visual Studio 2022 (or its Build Tools) with C++, CMake, Qt 6.8 with
# QtWebEngine for MSVC 2022, and NSIS for the installer; install-deps.ps1 installs
# all of them. `cmake --install` runs windeployqt, and this script adds the MSVC
# runtime beside the executable, so the result runs on a clean Windows 10/11 with
# nothing else installed.
param(
    [string]$Version = "",
    [string]$Qt = "",
    [string]$BuildDir = "build-windows",
    [string]$OutputDir = "dist",
    [string]$CertificateThumbprint = $env:SIGN_CERT_THUMBPRINT,
    [string]$TimestampUrl = "http://timestamp.digicert.com",
    [switch]$SkipBuild,
    [switch]$NoInstaller,
    [switch]$NoZip,
    [switch]$NoSmokeTest,
    [switch]$Help
)
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

if ($Help) {
    @"
Usage: build-windows.ps1 [options]

  -Version <x.y.z>               version in the file names (default: from CMakeLists.txt)
  -Qt <prefix>                   Qt to build against (default: .qt\, then CMAKE_PREFIX_PATH)
  -BuildDir <dir>                build directory (default build-windows)
  -OutputDir <dir>               where the packages go (default dist)
  -CertificateThumbprint <hash>  sign the executable and installer with this certificate
                                 from the certificate store (or env SIGN_CERT_THUMBPRINT)
  -TimestampUrl <url>            RFC 3161 timestamp server for signing
  -SkipBuild                     reuse the existing build directory as it is
  -NoInstaller / -NoZip          skip one of the two packages
  -NoSmokeTest                   do not launch the installed app to check it
"@
    exit 0
}

function Step($message) { Write-Host ""; Write-Host "==> $message" }
function Check($what) { if ($LASTEXITCODE -ne 0) { throw "$what failed (exit code $LASTEXITCODE)" } }

if (-not $Version) {
    $Version = (Select-String -Path CMakeLists.txt -Pattern '^\s*VERSION\s+([0-9][0-9.]*)\s*$' |
                Select-Object -First 1).Matches.Groups[1].Value
}
$VersionQuad = (@($Version.Split('.')) + @('0', '0', '0', '0'))[0..3] -join '.'

# ---- Find Qt ------------------------------------------------------------------------

if (-not $Qt) {
    $config = Get-ChildItem -Path ".qt" -Recurse -Depth 5 -Filter "Qt6WebEngineWidgetsConfig.cmake" `
        -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($config) { $Qt = (Resolve-Path (Join-Path $config.DirectoryName "..\..\..")).Path }
}
if (-not $Qt -and $env:CMAKE_PREFIX_PATH) { $Qt = $env:CMAKE_PREFIX_PATH.Split(';')[0] }
if (-not $Qt -or -not (Test-Path (Join-Path $Qt "bin\Qt6Core.dll"))) {
    throw "No Qt for MSVC found. Run install-deps.ps1, or pass -Qt C:\Qt\6.8.3\msvc2022_64"
}
$env:Path = "$(Join-Path $Qt 'bin');$env:Path"

$Root = (Get-Location).Path
$Dist = Join-Path $Root (Join-Path $BuildDir "dist")      # windeployqt wants an absolute prefix
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = (Resolve-Path $OutputDir).Path
$BaseName = "SpacePenguin-$Version-windows-x64"

Write-Host "SpacePenguin $Version, Qt from $Qt"

# Compiler jobs this machine has memory for: each needs about 500 MB, so allow
# one per GB free, at most one per CPU. SPACEPENGUIN_BUILD_JOBS overrides it.
function Get-BuildJobs {
    if ($env:SPACEPENGUIN_BUILD_JOBS) { return [int]$env:SPACEPENGUIN_BUILD_JOBS }
    $freeGB = [math]::Floor((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1MB)
    return [math]::Max(1, [math]::Min([Environment]::ProcessorCount, $freeGB))
}

# ---- Build and deploy -----------------------------------------------------------------

if (-not $SkipBuild) {
    Step "Building"
    cmake -S . -B $BuildDir -A x64 -DCMAKE_PREFIX_PATH="$Qt" -DSPACEPENGUIN_BUILD_TESTS=OFF
    Check "Configuring"
    cmake --build $BuildDir --config Release --parallel (Get-BuildJobs)
    Check "Building"
}

Step "Installing and deploying Qt (windeployqt)"
if (Test-Path $Dist) { Remove-Item -Recurse -Force $Dist }
cmake --install $BuildDir --config Release --prefix $Dist
Check "Installing"

# ---- MSVC runtime -------------------------------------------------------------------------
# Copied beside the executable ("app-local" deployment, which Microsoft supports),
# so no VC++ redistributable has to be installed first.

Step "Adding the MSVC runtime"
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
$crt = Get-ChildItem -Path (Join-Path $vsPath "VC\Redist\MSVC") -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    ForEach-Object { Get-ChildItem -Path (Join-Path $_.FullName "x64") -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue } |
    Select-Object -First 1
if (-not $crt) { throw "The MSVC runtime (VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT) was not found under $vsPath" }
Copy-Item -Path (Join-Path $crt.FullName "*.dll") -Destination $Dist -Force
Write-Host "    from $($crt.FullName)"

# ---- Check the deployment ---------------------------------------------------------------

Step "Checking the deployment"
foreach ($file in "SpacePenguin.exe", "QtWebEngineProcess.exe", "Qt6WebEngineCore.dll", "Qt6Widgets.dll",
                  "qwindows.dll", "qtwebengine_resources.pak", "icudtl.dat", "vcruntime140.dll",
                  "msvcp140.dll") {
    $found = Get-ChildItem -Path $Dist -Recurse -Filter $file | Select-Object -First 1
    if (-not $found) { throw "Missing from the deployment: $file" }
}
Write-Host "    everything SpacePenguin needs is in $Dist"

if (-not $NoSmokeTest) {
    Step "Smoke test: launching the app"
    # Print --version to the console instead of a message box.
    $env:QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES = "1"
    $out = Join-Path $env:TEMP "spacepenguin-version.txt"
    $process = Start-Process -FilePath (Join-Path $Dist "SpacePenguin.exe") -ArgumentList "--version" `
        -RedirectStandardOutput $out -NoNewWindow -PassThru
    if (-not $process.WaitForExit(120000)) { $process.Kill(); throw "SpacePenguin.exe --version did not exit" }
    $reported = (Get-Content $out -Raw).Trim()
    Write-Host "    $reported"
    if ($reported -notlike "*SpacePenguin $Version*") {
        throw "The app did not start correctly (expected 'SpacePenguin $Version', got '$reported')"
    }
}

# ---- Sign --------------------------------------------------------------------------------

function Sign($file) {
    if (-not $CertificateThumbprint) { return }
    $signtool = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse `
        -Filter signtool.exe | Where-Object { $_.FullName -like "*\x64\*" } |
        Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $signtool) { throw "signtool.exe not found; install the Windows SDK" }
    & $signtool.FullName sign /sha1 $CertificateThumbprint /fd sha256 /tr $TimestampUrl /td sha256 $file
    Check "Signing $file"
}
if ($CertificateThumbprint) {
    Step "Signing the executable"
    Sign (Join-Path $Dist "SpacePenguin.exe")
}

# ---- Package -------------------------------------------------------------------------------

$packages = @()

if (-not $NoZip) {
    Step "Creating the portable zip"
    $zip = Join-Path $OutputDir "$BaseName.zip"
    if (Test-Path $zip) { Remove-Item $zip }
    # Inside the zip everything sits in a SpacePenguin folder.
    $staging = Join-Path $env:TEMP "spacepenguin-zip"
    if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
    New-Item -ItemType Directory -Path $staging | Out-Null
    Copy-Item -Recurse -Path $Dist -Destination (Join-Path $staging "SpacePenguin")
    Compress-Archive -Path (Join-Path $staging "SpacePenguin") -DestinationPath $zip -CompressionLevel Optimal
    Remove-Item -Recurse -Force $staging
    $packages += $zip
}

if (-not $NoInstaller) {
    Step "Creating the installer (NSIS)"
    $makensis = Get-Command makensis -ErrorAction SilentlyContinue
    if (-not $makensis) {
        $candidate = Join-Path ${env:ProgramFiles(x86)} "NSIS\makensis.exe"
        if (Test-Path $candidate) { $makensis = Get-Item $candidate }
    }
    if (-not $makensis) { throw "makensis (NSIS 3) not found; run install-deps.ps1 or pass -NoInstaller" }
    $makensisPath = if ($makensis.Source) { $makensis.Source } else { $makensis.FullName }
    & $makensisPath /V2 /WX "/DVERSION=$Version" "/DVERSION_QUAD=$VersionQuad" "/DSOURCE_DIR=$Dist" `
        "/DOUTPUT_DIR=$OutputDir" installer.nsi
    Check "makensis"
    $installer = Join-Path $OutputDir "$BaseName-setup.exe"
    Sign $installer
    $packages += $installer
}

$sums = foreach ($package in $packages) {
    "$((Get-FileHash -Algorithm SHA256 $package).Hash.ToLower())  $(Split-Path -Leaf $package)"
}
$sums | Out-File -Encoding ascii (Join-Path $OutputDir "$BaseName.sha256")

Step "Created"
foreach ($package in $packages) {
    Write-Host ("    {0} ({1:N0} MB)" -f $package, ((Get-Item $package).Length / 1MB))
}
if (-not $CertificateThumbprint) {
    Write-Host "    Unsigned: Windows SmartScreen will ask before the first run."
}
