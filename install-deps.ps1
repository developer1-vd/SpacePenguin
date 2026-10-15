# Install everything needed to build, test and package SpacePenguin on Windows.
# On Linux and macOS use install-deps.sh.
#
#   powershell -ExecutionPolicy Bypass -File install-deps.ps1
#
# Installs, with winget (or Chocolatey when winget is missing):
#   - Visual Studio 2022 Build Tools with the C++ workload, unless MSVC is present
#   - CMake, Ninja, Python 3 and NSIS (for the installer)
#   - Qt 6.8 with QtWebEngine for MSVC 2022, through aqtinstall, into .qt\ in the
#     repository, where CMake finds it without any extra flags
param(
    [string]$QtVersion = "6.8.3",
    [string]$QtDir = (Join-Path $PSScriptRoot ".qt"),
    [switch]$SkipVisualStudio,
    [switch]$NoPackaging,
    [switch]$DryRun
)
$ErrorActionPreference = "Stop"

$QtModules = @("qtwebengine", "qtwebchannel", "qtpositioning", "qtpdf", "qtserialport")
$QtArch = "win64_msvc2022_64"

function Step($message) { Write-Host ""; Write-Host "==> $message" }
function Run([string]$exe, [string[]]$arguments) {
    Write-Host "    $ $exe $($arguments -join ' ')"
    if ($DryRun) { return }
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$exe exited with code $LASTEXITCODE" }
}
function Have($command) { [bool](Get-Command $command -ErrorAction SilentlyContinue) }

# Picks up PATH changes made by the installers without opening a new shell.
function Update-SessionPath {
    $machine = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $user = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machine;$user"
}

$UseWinget = Have "winget"
if (-not $UseWinget -and -not (Have "choco")) {
    throw "Neither winget nor Chocolatey is available. Install 'App Installer' from the Microsoft Store (for winget), then run this script again."
}

function Install-Tool($wingetId, $chocoId, $command, $extra = @()) {
    if ($command -and (Have $command)) {
        Write-Host "    $command is already installed"
        return
    }
    if ($UseWinget) {
        Run "winget" (@("install", "--id", $wingetId, "--exact", "--silent",
                        "--accept-package-agreements", "--accept-source-agreements") + $extra)
    } else {
        Run "choco" @("install", $chocoId, "-y", "--no-progress")
    }
    Update-SessionPath
}

# ---- Visual Studio ------------------------------------------------------------------

function Find-Msvc {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }
    $path = & $vswhere -latest -products * -version "[17.0,18.0)" `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($path) { return $path }
    return $null
}

if (-not $SkipVisualStudio) {
    Step "Checking for Visual Studio 2022 with C++"
    $msvc = Find-Msvc
    if ($msvc) {
        Write-Host "    found $msvc"
    } else {
        Write-Host "    installing Visual Studio 2022 Build Tools (C++ workload, several GB)"
        $vsArgs = "--wait --quiet --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
        if ($UseWinget) {
            Run "winget" @("install", "--id", "Microsoft.VisualStudio.2022.BuildTools", "--exact",
                           "--silent", "--accept-package-agreements", "--accept-source-agreements",
                           "--override", $vsArgs)
        } else {
            Run "choco" @("install", "visualstudio2022buildtools", "-y", "--no-progress",
                          "--package-parameters", $vsArgs)
        }
    }
}

# ---- Tools ---------------------------------------------------------------------------

Step "Installing CMake, Ninja and Python"
Install-Tool "Kitware.CMake" "cmake" "cmake" @("--override", "ADD_CMAKE_TO_PATH=System /quiet")
Install-Tool "Ninja-build.Ninja" "ninja" "ninja"
if (-not (Have "python") -or ((& python --version 2>&1) -notmatch "Python 3")) {
    Install-Tool "Python.Python.3.12" "python312" $null
}

if (-not $NoPackaging) {
    Step "Installing NSIS for the installer"
    $nsisDir = Join-Path ${env:ProgramFiles(x86)} "NSIS"
    if ((Have "makensis") -or (Test-Path (Join-Path $nsisDir "makensis.exe"))) {
        Write-Host "    NSIS is already installed"
    } else {
        Install-Tool "NSIS.NSIS" "nsis" $null
    }
}

# ---- Qt ------------------------------------------------------------------------------

function Find-QtPrefix {
    $config = Get-ChildItem -Path (Join-Path $QtDir $QtVersion) -Recurse -Depth 4 `
        -Filter "Qt6WebEngineWidgetsConfig.cmake" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($config) { return (Resolve-Path (Join-Path $config.DirectoryName "..\..\..")).Path }
    return $null
}

$QtPrefix = Find-QtPrefix
if ($QtPrefix) {
    Step "Qt $QtVersion with QtWebEngine is already in $QtPrefix"
} else {
    Step "Installing Qt $QtVersion ($QtArch) with QtWebEngine into $QtDir"
    Write-Host "    This downloads about 1.5 GB from the Qt mirrors."
    $python = if (Have "py") { "py" } else { "python" }
    $venv = Join-Path $QtDir ".venv"
    $pythonArgs = if ($python -eq "py") { @("-3", "-m", "venv", $venv) } else { @("-m", "venv", $venv) }
    Run $python $pythonArgs
    Run (Join-Path $venv "Scripts\python.exe") @("-m", "pip", "install", "--quiet", "--upgrade", "pip", "aqtinstall")
    Run (Join-Path $venv "Scripts\aqt.exe") (@("install-qt", "windows", "desktop", $QtVersion, $QtArch,
                                               "--outputdir", $QtDir, "--modules") + $QtModules)
    if (-not $DryRun) { $QtPrefix = Find-QtPrefix }
}

# CI: put Qt's DLLs and tools on PATH for the following steps (tests need the DLLs).
if ($env:GITHUB_PATH -and $QtPrefix) {
    (Join-Path $QtPrefix "bin") | Out-File -FilePath $env:GITHUB_PATH -Append -Encoding utf8
}

Step "Done"
Write-Host @"
Qt is in $QtPrefix. CMake picks up Qt from .qt\ by itself. In a new
terminal (so the freshly installed tools are on PATH):

    cmake -B build
    cmake --build build --config Release --parallel

To run the browser or the tests from the build tree, put Qt on PATH first:

    `$env:Path = "$QtPrefix\bin;`$env:Path"
    ctest --test-dir build -C Release --output-on-failure
    build\Release\SpacePenguin.exe

To build the installer and the portable zip:

    .\build-windows.ps1 -Version 0.1.0
"@
