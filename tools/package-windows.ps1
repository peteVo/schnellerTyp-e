<#
.SYNOPSIS
    Build schnellerTyp-e in Release and produce a portable .zip for GitHub.

.DESCRIPTION
    Produces dist\schnellerTyp-e-<version>-windows-x64.zip: a folder your friend
    unzips anywhere and double-clicks. No installer, no admin rights, no
    prerequisites on their machine — the Qt DLLs, the QML runtime, uiohook.dll
    and the Visual C++ runtime all travel inside the zip.

    The script refuses to produce a package it has not verified. After staging
    it launches the executable from the staged folder and waits for it to reach
    its event loop, which is what catches the classic failure: a build that runs
    fine on the machine that made it and dies with "the code execution cannot
    proceed because Qt6Core.dll was not found" on every other machine.

.PARAMETER QtDir
    Qt installation for MSVC x64. Default matches this project's Qt Creator kit.

.PARAMETER UiohookRoot
    Where libuiohook was installed (the folder containing lib\ and bin\).

.PARAMETER SkipSmokeTest
    Skip the launch check. Only useful if an instance is already running and
    you would rather not quit it.

.EXAMPLE
    .\tools\package-windows.ps1

.EXAMPLE
    .\tools\package-windows.ps1 -QtDir C:\Qt\6.8.0\msvc2022_64
#>

#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$QtDir       = 'C:\Qt\6.11.2\msvc2022_64',
    [string]$UiohookRoot = 'C:\libs\uiohook',
    [string]$SourceDir   = '',
    [string]$BuildDir    = '',
    [string]$OutRoot     = '',
    [switch]$SkipSmokeTest
)

$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'   # Compress-Archive is far faster without it

function Write-Step($text) { Write-Host "`n==> $text" -ForegroundColor Cyan }
function Write-Ok($text)   { Write-Host "    $text" -ForegroundColor Green }
function Write-Note($text) { Write-Host "    $text" -ForegroundColor DarkGray }

# Stop with an explanation rather than a stack trace. `throw` flattens a
# multi-line message into one line and wraps it in PowerShell's exception
# formatting, which buries the sentence that tells you what to do.
function Fail($text) {
    Write-Host ''
    Write-Host $text -ForegroundColor Red
    Write-Host ''
    exit 1
}

# ---------------------------------------------------------------------------
# Locations
# ---------------------------------------------------------------------------

if (-not $SourceDir) { $SourceDir = Split-Path -Parent $PSScriptRoot }
$SourceDir = (Resolve-Path $SourceDir).Path
if (-not $BuildDir) { $BuildDir = Join-Path $SourceDir 'build\release-package' }
if (-not $OutRoot)  { $OutRoot  = Join-Path $SourceDir 'dist' }

Write-Step 'Checking prerequisites'

$windeployqt = Join-Path $QtDir 'bin\windeployqt.exe'
if (-not (Test-Path $windeployqt)) {
    Fail @"
windeployqt.exe not found under $QtDir.

Pass the right Qt with -QtDir. It must be the MSVC 64-bit build, e.g.
    .\tools\package-windows.ps1 -QtDir C:\Qt\6.11.2\msvc2022_64
A MinGW Qt cannot be used: this project is built with MSVC and the two
toolchains produce incompatible binaries.
"@
}
Write-Ok "Qt:       $QtDir"

$uiohookLib = Join-Path $UiohookRoot 'lib\uiohook.lib'
if (-not (Test-Path $uiohookLib)) {
    Fail @"
uiohook.lib not found at $uiohookLib.

Pass the right location with -UiohookRoot, or build libuiohook first with
    .\tools\rebuild-libuiohook-x64.ps1
"@
}
Write-Ok "libuiohook: $UiohookRoot"

foreach ($tool in 'cmake', 'ninja') {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Fail "$tool is not on PATH. Open a terminal from Qt Creator, or add Qt's Tools\CMake_64\bin and Tools\Ninja to PATH."
    }
}

# Version comes from CMakeLists.txt so the zip name, the file properties and
# the git tag can never disagree.
$cmakeLists = Get-Content (Join-Path $SourceDir 'CMakeLists.txt') -Raw
if ($cmakeLists -notmatch '(?s)project\s*\(\s*schnellerTyp-e.*?VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    Fail "Could not read VERSION from CMakeLists.txt."
}
$version = $Matches[1]
Write-Ok "version:  $version"

# ---------------------------------------------------------------------------
# Visual Studio environment
#
# The Ninja generator needs cl.exe, link.exe and rc.exe on PATH. Rather than
# asking you to remember to start a "x64 Native Tools" prompt, find the
# installation and enter its developer shell here.
# ---------------------------------------------------------------------------

Write-Step 'Entering the Visual Studio x64 developer shell'

if ($env:VSCMD_ARG_TGT_ARCH -eq 'x64') {
    Write-Ok 'Already in an x64 developer shell.'
} else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        Fail "vswhere.exe not found. Is Visual Studio with the C++ workload installed?"
    }
    $vsPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsPath) {
        Fail "No Visual Studio installation with the C++ build tools was found."
    }
    $devShell = Join-Path $vsPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
    if (-not (Test-Path $devShell)) {
        Fail "Visual Studio at $vsPath has no DevShell module; cannot set up the compiler environment."
    }
    Import-Module $devShell
    Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation `
        -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
    Write-Ok "Visual Studio: $vsPath"
}

if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    Fail "The developer shell did not come up as x64 (VSCMD_ARG_TGT_ARCH='$env:VSCMD_ARG_TGT_ARCH'). A 32-bit build will not link against the x64 uiohook.lib."
}

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

Write-Step "Configuring Release in $BuildDir"

# A fresh directory on purpose. Reusing a Debug tree is how you end up shipping
# debug Qt DLLs, which will not run on a machine without Visual Studio.
if (Test-Path $BuildDir) { Remove-Item $BuildDir -Recurse -Force }

& cmake -S $SourceDir -B $BuildDir -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$QtDir" `
    "-DUIOHOOK_ROOT=$UiohookRoot"
if ($LASTEXITCODE -ne 0) { Fail "CMake configure failed." }

Write-Step 'Building'
& cmake --build $BuildDir --parallel
if ($LASTEXITCODE -ne 0) { Fail "Build failed." }

$builtExe = Join-Path $BuildDir 'schnellerTyp-e.exe'
if (-not (Test-Path $builtExe)) { Fail "Build reported success but $builtExe does not exist." }
Write-Ok "built: $builtExe"

Write-Step 'Running the unit tests'
& ctest --test-dir $BuildDir --output-on-failure
if ($LASTEXITCODE -ne 0) { Fail "Tests failed. Not packaging a build that does not pass its own tests." }

# ---------------------------------------------------------------------------
# Stage
# ---------------------------------------------------------------------------

$packageName = "schnellerTyp-e-$version-windows-x64"
$stageDir    = Join-Path $OutRoot $packageName

Write-Step "Staging into $stageDir"
if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force }
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

Copy-Item $builtExe $stageDir

Write-Step 'Collecting the Qt runtime (windeployqt)'
# --qmldir is what makes this work for a QML application: without it windeployqt
# has no way to know which QML modules the UI imports, and the app starts to a
# blank window on any machine that does not have Qt installed.
& $windeployqt `
    --release `
    --qmldir (Join-Path $SourceDir 'qml') `
    --compiler-runtime `
    --no-system-d3d-compiler `
    (Join-Path $stageDir 'schnellerTyp-e.exe')
if ($LASTEXITCODE -ne 0) { Fail "windeployqt failed." }

Write-Step 'Adding uiohook.dll'
$uiohookDll = @(
    (Join-Path $UiohookRoot 'bin\uiohook.dll'),
    (Join-Path $UiohookRoot 'lib\uiohook.dll'),
    (Join-Path $BuildDir 'uiohook.dll')
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $uiohookDll) {
    Fail "uiohook.dll not found under $UiohookRoot or in the build tree. The package would fail to start on any machine."
}
Copy-Item $uiohookDll $stageDir -Force
Write-Ok "from: $uiohookDll"

# --- Visual C++ runtime ----------------------------------------------------
#
# windeployqt --compiler-runtime usually handles this, but which files it copies
# has changed between Qt versions and it is the single most common reason a
# package works on the build machine and nowhere else. So: check, and fill the
# gap from the Visual Studio redistributable if anything is missing.

Write-Step 'Checking the Visual C++ runtime'
$crtNeeded = @('vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140.dll')
$crtMissing = $crtNeeded | Where-Object { -not (Test-Path (Join-Path $stageDir $_)) }

if ($crtMissing) {
    Write-Note "windeployqt did not supply: $($crtMissing -join ', ')"
    if (-not $env:VCToolsRedistDir) { Fail "VCToolsRedistDir is not set; cannot locate the redistributable DLLs." }
    $crtDir = Get-ChildItem (Join-Path $env:VCToolsRedistDir 'x64') -Filter 'Microsoft.VC*.CRT' -Directory |
              Select-Object -First 1
    if (-not $crtDir) { Fail "No Microsoft.VC*.CRT folder under $env:VCToolsRedistDir\x64." }
    foreach ($dll in $crtMissing) {
        $source = Join-Path $crtDir.FullName $dll
        if (-not (Test-Path $source)) { Fail "$dll not found in $($crtDir.FullName)." }
        Copy-Item $source $stageDir -Force
    }
    Write-Ok "copied from $($crtDir.FullName)"
} else {
    Write-Ok 'Supplied by windeployqt.'
}

$stillMissing = $crtNeeded | Where-Object { -not (Test-Path (Join-Path $stageDir $_)) }
if ($stillMissing) { Fail "Visual C++ runtime still incomplete: $($stillMissing -join ', ')" }

# --- documents and licences ------------------------------------------------

Write-Step 'Adding documents and licences'
foreach ($doc in 'README.md', 'BUILD.md', 'NOTICE.md', 'LICENSE') {
    $path = Join-Path $SourceDir $doc
    if (Test-Path $path) { Copy-Item $path $stageDir }
}

# Qt and libuiohook are LGPL-3.0 and are shipped here as separate DLLs, which is
# what keeps that simple: the licence text has to travel with them.
$licenceDir = Join-Path $stageDir 'licenses'
New-Item -ItemType Directory -Path $licenceDir -Force | Out-Null
$qtLicences = Join-Path (Split-Path -Parent (Split-Path -Parent $QtDir)) 'Licenses'
if (Test-Path $qtLicences) {
    Copy-Item (Join-Path $qtLicences '*') $licenceDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Ok "Qt licence texts from $qtLicences"
} else {
    Write-Note "Qt's Licenses folder not found at $qtLicences; writing a pointer instead."
}
@"
schnellerTyp-e itself is MIT licensed; see LICENSE and NOTICE.md.

This package also contains, as separate DLLs:

  Qt 6          LGPL-3.0   https://www.qt.io/licensing  -  https://download.qt.io/official_releases/qt/
  libuiohook    LGPL-3.0   https://github.com/kwhat/libuiohook

Both are dynamically linked and can be replaced with your own builds by
substituting the DLLs in this folder, which is what the LGPL asks for. Full
licence texts are alongside this file where the build machine had them.
"@ | Set-Content (Join-Path $licenceDir 'README.txt') -Encoding UTF8

# --- first-run note --------------------------------------------------------

@"
schnellerTyp-e $version — portable build for Windows x64
=========================================================

1. Unzip this folder anywhere you like (Desktop, Documents, a USB stick).
2. Double-click schnellerTyp-e.exe.

Windows will most likely show a blue "Windows protected your PC" box the first
time. That is SmartScreen reacting to a program it has never seen before, not a
virus warning. Click "More info", then "Run anyway". It appears once.

The app has no window of its own: look for the purple ST badge in the system
tray, next to the clock. If it is hidden, click the ^ arrow and drag it out.

  Left click        pause / resume
  Middle click      next language
  Ctrl + Shift      next language, or Off  (tap and release, nothing else)
  Double click      open settings
  Right click       menu

Everything is in this folder — nothing is installed, nothing is written to the
registry except your settings. To remove it: quit from the tray menu, then
delete this folder.

If it will not start, the log says why:
  %LOCALAPPDATA%\schnellerTyp-e\schnellerTyp-e\schnellerTyp-e.log

BUILD.md has the full guide.
"@ | Set-Content (Join-Path $stageDir 'READ-ME-FIRST.txt') -Encoding UTF8

# ---------------------------------------------------------------------------
# Verify
# ---------------------------------------------------------------------------

if ($SkipSmokeTest) {
    Write-Step 'Skipping the smoke test (-SkipSmokeTest)'
} else {
    Write-Step 'Smoke test: launching the staged executable'

    $logPath = Join-Path $env:LOCALAPPDATA 'schnellerTyp-e\schnellerTyp-e\schnellerTyp-e.log'
    if (Test-Path $logPath) { Remove-Item $logPath -Force -ErrorAction SilentlyContinue }

    # --no-hook so packaging never installs a keyboard hook; --no-tray so this
    # leaves no icon behind when it is killed.
    $proc = Start-Process -FilePath (Join-Path $stageDir 'schnellerTyp-e.exe') `
                          -ArgumentList '--no-hook', '--no-tray' -PassThru
    Start-Sleep -Seconds 6

    $exited = $proc.HasExited
    $log    = if (Test-Path $logPath) { Get-Content $logPath -Raw } else { '' }
    if (-not $exited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }

    if ($exited -and $log -match 'another instance is already running') {
        Fail "A copy of schnellerTyp-e is already running, so the smoke test could not check this build. Quit it from the tray and run this script again (or pass -SkipSmokeTest)."
    }
    if ($exited) {
        Fail @"
The staged executable exited immediately — this package would not work.

Log ($logPath):
$log
"@
    }
    if ($log -notmatch 'entering event loop') {
        Fail @"
The staged executable started but never reached its event loop.

Log ($logPath):
$log
"@
    }
    Write-Ok 'Started, loaded its QML and reached the event loop.'
}

# ---------------------------------------------------------------------------
# Zip
# ---------------------------------------------------------------------------

Write-Step 'Compressing'
$zipPath = Join-Path $OutRoot "$packageName.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path $stageDir -DestinationPath $zipPath -CompressionLevel Optimal

$sizeMb = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)
$sha    = (Get-FileHash $zipPath -Algorithm SHA256).Hash

Write-Host ""
Write-Host "Package ready" -ForegroundColor Green
Write-Host "  file    $zipPath"
Write-Host "  size    $sizeMb MB"
Write-Host "  sha256  $sha"
Write-Host ""
Write-Host "Publish it with:" -ForegroundColor Cyan
Write-Host "  git tag -a v$version -m ""schnellerTyp-e $version"""
Write-Host "  git push origin v$version"
Write-Host "  gh release create v$version ""$zipPath"" --title ""schnellerTyp-e $version"" --notes-file RELEASE-NOTES.md"
Write-Host ""
Write-Host "See RELEASE.md for the whole procedure." -ForegroundColor DarkGray
