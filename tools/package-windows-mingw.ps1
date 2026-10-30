#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$SourceDir = '',
    [string]$BuildDir = '',
    [string]$QtDir = 'C:\Qt\6.11.2\mingw_64',
    [string]$MinGWBin = 'C:\Qt\Tools\mingw1310_64\bin',
    [string]$UiohookDll = '',
    [string]$OutRoot = ''
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

function Fail([string]$message) {
    throw $message
}

if (-not $SourceDir) { $SourceDir = Split-Path -Parent $PSScriptRoot }
$SourceDir = (Resolve-Path $SourceDir).Path
if (-not $BuildDir) { $BuildDir = Join-Path $SourceDir 'build\app-mingw' }
if (-not $OutRoot) { $OutRoot = Join-Path $SourceDir 'dist' }
$BuildDir = (Resolve-Path $BuildDir).Path
$OutRoot = [IO.Path]::GetFullPath($OutRoot)

$exe = Join-Path $BuildDir 'schnellerTyp-e.exe'
$windeployqt = Join-Path $QtDir 'bin\windeployqt.exe'
if (-not (Test-Path $exe)) { Fail "Built executable not found: $exe" }
if (-not (Test-Path $windeployqt)) { Fail "windeployqt.exe not found: $windeployqt" }
if (-not (Test-Path $MinGWBin)) { Fail "MinGW runtime folder not found: $MinGWBin" }
if (-not $UiohookDll) {
    $UiohookDll = Join-Path $SourceDir 'build\uiohook-mingw-install\bin\libuiohook.dll'
}
if (-not (Test-Path $UiohookDll)) { Fail "libuiohook DLL not found: $UiohookDll" }

$cmakeLists = Get-Content (Join-Path $SourceDir 'CMakeLists.txt') -Raw
if ($cmakeLists -notmatch '(?s)project\s*\(\s*schnellerTyp-e.*?VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    Fail 'Could not read the project version from CMakeLists.txt.'
}
$version = $Matches[1]
$packageName = "schnellerTyp-e-$version-windows-x64-mingw"
$stageDir = Join-Path $OutRoot $packageName
$zipPath = Join-Path $OutRoot "$packageName.zip"

# Never remove prior output: each package run gets a fresh, versioned folder.
if (Test-Path $stageDir) { Fail "Staging folder already exists; move it aside before retrying: $stageDir" }
if (Test-Path $zipPath) { Fail "Package already exists; move it aside before retrying: $zipPath" }
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

try {
    Copy-Item $exe $stageDir

    $env:PATH = "$($QtDir)\bin;$MinGWBin;$env:PATH"
    & $windeployqt --release --qmldir (Join-Path $SourceDir 'qml') --compiler-runtime `
        (Join-Path $stageDir 'schnellerTyp-e.exe')
    if ($LASTEXITCODE -ne 0) { Fail "windeployqt failed with exit code $LASTEXITCODE." }

    Copy-Item $UiohookDll (Join-Path $stageDir 'libuiohook.dll')
    foreach ($runtime in 'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll') {
        $runtimePath = Join-Path $MinGWBin $runtime
        if (-not (Test-Path $runtimePath)) { Fail "Required MinGW runtime DLL not found: $runtimePath" }
        Copy-Item $runtimePath $stageDir -Force
    }

    foreach ($document in 'README.md', 'BUILD.md', 'PLATFORMS.md', 'VERIFICATION.md', 'NOTICE.md', 'LICENSE') {
        $path = Join-Path $SourceDir $document
        if (Test-Path $path) { Copy-Item $path $stageDir }
    }
    $licenses = Join-Path $stageDir 'licenses'
    $wordfreq = Join-Path $licenses 'wordfreq'
    $uiohookLicenses = Join-Path $licenses 'libuiohook'
    New-Item -ItemType Directory -Path $wordfreq,$uiohookLicenses -Force | Out-Null
    Copy-Item (Join-Path $SourceDir 'src\core\auto\data\LICENSE-CC-BY-SA-4.0.txt') $wordfreq
    Copy-Item (Join-Path $SourceDir 'src\core\auto\data\NOTICE-wordfreq.md') $wordfreq
    Copy-Item (Join-Path $SourceDir 'build\libuiohook-src\COPYING.LESSER.md') $uiohookLicenses
    Copy-Item (Join-Path $SourceDir 'build\libuiohook-src\COPYING.md') $uiohookLicenses
    Copy-Item (Join-Path $SourceDir 'build\libuiohook-src\COPYING.LESSER.md') `
        (Join-Path $licenses 'Qt-LGPL-3.0.txt')
    @'
Qt 6 is dynamically linked and uses the LGPL-3.0 license text included beside
this file. Qt licensing information: https://www.qt.io/licensing
'@ | Set-Content (Join-Path $licenses 'Qt-LICENSE.txt') -Encoding UTF8

    @"
schnellerTyp-e $version — portable Windows x64 build (MinGW)
============================================================

1. Unzip the folder and double-click schnellerTyp-e.exe.
2. Choose Automatic in the language menu or settings for word-by-word German,
   Vietnamese, English or literal text selection.
3. The current word can change while you type. Space commits the selection;
   Escape restores the original keys for the current word.

The app lives in the system tray. Left click pauses or resumes it; double click
opens settings; right click opens its menu. Ctrl + Shift cycles languages.
Nothing is installed. Quit from the tray, then delete this folder to remove it.

The offline word frequency data attribution and CC BY-SA 4.0 license are in
licenses\wordfreq. libuiohook license texts are in licenses\libuiohook.
"@ | Set-Content (Join-Path $stageDir 'READ-ME-FIRST.txt') -Encoding UTF8

    Compress-Archive -Path $stageDir -DestinationPath $zipPath -CompressionLevel Optimal
} catch {
    if (Test-Path $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    throw
}

$sha = (Get-FileHash $zipPath -Algorithm SHA256).Hash
$size = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)
Write-Host "Package: $zipPath"
Write-Host "Stage:   $stageDir"
Write-Host "Size:    $size MB"
Write-Host "SHA256:  $sha"
