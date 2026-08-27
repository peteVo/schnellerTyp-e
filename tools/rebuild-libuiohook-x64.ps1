<#
.SYNOPSIS
    Build and install a 64-bit libuiohook for schnellerTyp-e.

.DESCRIPTION
    Clones libuiohook if needed, enters the Visual Studio x64 developer shell,
    configures a fresh Release build tree, installs, and then verifies both the
    architecture and the runtime of what it produced.

    Two things this guards against, both of which have actually happened here:

      * a 32-bit libuiohook against a 64-bit schnellerTyp-e, which surfaces as
        four unresolved hook_* symbols with one easily missed LNK4272 warning
        above them. The architecture comes from the developer shell, and a
        fresh build tree is used because a CMake tree cannot change platform in
        place;

      * a *Debug* libuiohook, which is the right architecture and still
        unusable: it imports the debug C runtime, which Microsoft does not
        redistribute, so the DLL works on the build machine and nowhere else.

.PARAMETER Source
    Where to clone/find the libuiohook sources. Default C:\src\libuiohook

.PARAMETER Prefix
    Install prefix. This is the value to give schnellerTyp-e as UIOHOOK_ROOT.
    Default C:\libs\uiohook

.PARAMETER Static
    Build a static library instead of a DLL. Note that libuiohook is LGPL, so a
    static link carries relinking obligations if you distribute the result.

.EXAMPLE
    .\rebuild-libuiohook-x64.ps1

.EXAMPLE
    .\rebuild-libuiohook-x64.ps1 -Source D:\src\libuiohook -Prefix D:\libs\uiohook

.NOTES
    A plain PowerShell is fine — the script finds Visual Studio and enters its
    x64 developer shell itself.
#>

[CmdletBinding()]
param(
    [string] $Source = 'C:\src\libuiohook',
    [string] $Prefix = 'C:\libs\uiohook',
    [switch] $Static
)

$ErrorActionPreference = 'Stop'

function Invoke-Step {
    param(
        [Parameter(Mandatory)] [string]   $Name,
        [Parameter(Mandatory)] [string[]] $Command
    )
    Write-Host ""
    Write-Host "==> $Name" -ForegroundColor Cyan
    Write-Host "    $($Command -join ' ')" -ForegroundColor DarkGray

    $exe = $Command[0]
    # Splat from a variable. @(...) inline would build an array rather than
    # splat it, which only happens to work for native commands and mangles any
    # argument containing a space. Note also that 1..0 counts *down*, so an
    # empty tail has to be handled explicitly.
    $argv = if ($Command.Length -gt 1) { $Command[1..($Command.Length - 1)] } else { @() }

    & $exe @argv
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE."
    }
}

# --- prerequisites ----------------------------------------------------------

foreach ($tool in 'cmake', 'git') {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool is not on PATH. Add it (Qt ships CMake and Ninja under C:\Qt\Tools) and try again."
    }
}

# Windows keeps a loaded DLL locked, so `cmake --install` cannot replace
# uiohook.dll while schnellerTyp-e has it mapped — it fails partway with
# "Access is denied", leaving the prefix half-updated. Cheaper to say so now
# than to have someone decode that error afterwards.
$running = @(Get-Process -Name 'schnellerTyp-e' -ErrorAction SilentlyContinue)
if ($running.Count -gt 0) {
    throw @"
schnellerTyp-e is running (PID $($running.Id -join ', ')).

Windows locks uiohook.dll while it is loaded, so installing over it would fail.
Quit the app from its tray icon — right-click the ST badge, "Quit
schnellerTyp-e" — and run this again.
"@
}

# Enter the Visual Studio x64 developer shell rather than warning that we are
# not in one. Warning and continuing was worse than useless: CMake then failed
# with "CMAKE_C_COMPILER not set", which reads like a CMake problem rather than
# a missing toolchain.
#
# This block is deliberately a copy of the one in package-windows.ps1 rather
# than a shared helper — that one is known to work on the machines this project
# is built on, and a refactor here would put both at risk to save twenty lines.
if ($env:VSCMD_ARG_TGT_ARCH -eq 'x64') {
    Write-Host "==> Already in an x64 developer shell" -ForegroundColor Cyan
} else {
    Write-Host ""
    Write-Host "==> Entering the Visual Studio x64 developer shell" -ForegroundColor Cyan
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found. Is Visual Studio with the C++ workload installed?"
    }
    $vsPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsPath) { throw "No Visual Studio installation with the C++ build tools was found." }
    $devShell = Join-Path $vsPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
    if (-not (Test-Path $devShell)) {
        throw "Visual Studio at $vsPath has no DevShell module; cannot set up the compiler environment."
    }
    Import-Module $devShell
    Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation `
        -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
    Write-Host "    $vsPath" -ForegroundColor DarkGray
}

if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    throw "The developer shell did not come up as x64 (VSCMD_ARG_TGT_ARCH='$env:VSCMD_ARG_TGT_ARCH')."
}

# Single-config generators only, so that CMAKE_BUILD_TYPE decides Release and
# there is no way to end up with a Debug artifact from a multi-config tree.
# Ninja if it is around, NMake otherwise — the latter always is, inside a
# developer shell.
$generator = if (Get-Command 'ninja' -ErrorAction SilentlyContinue) { 'Ninja' } else { 'NMake Makefiles' }

$buildDir = Join-Path $Source 'build-x64'
$shared   = if ($Static) { 'OFF' } else { 'ON' }

Write-Host "libuiohook -> x64" -ForegroundColor Green
Write-Host "  source : $Source"
Write-Host "  build  : $buildDir   (recreated from scratch)"
Write-Host "  prefix : $Prefix"
Write-Host "  shared : $shared"

# --- sources ----------------------------------------------------------------

if (-not (Test-Path (Join-Path $Source '.git'))) {
    if (Test-Path $Source) {
        throw "$Source exists but is not a git clone. Move it aside or pass -Source somewhere else."
    }
    Invoke-Step 'Cloning libuiohook' @(
        'git', 'clone', 'https://github.com/kwhat/libuiohook.git', $Source
    )
} else {
    Write-Host ""
    Write-Host "==> Using existing clone at $Source" -ForegroundColor Cyan
}

# --- configure, build, install ---------------------------------------------

# A build tree remembers its platform, so start clean rather than trying to
# switch a 32-bit tree over to 64-bit.
if (Test-Path $buildDir) {
    Write-Host ""
    Write-Host "==> Removing the old build tree (platform cannot be changed in place)" -ForegroundColor Cyan
    Remove-Item -Recurse -Force $buildDir
}

# Note what is NOT here: -A x64. That flag only exists for the Visual Studio
# generators, and CMake defaults to Ninja on a machine that has it — which made
# this script fail outright with "Generator Ninja does not support platform
# specification". The architecture now comes from the developer shell entered
# above, which is where it belonged all along.
Invoke-Step "Configuring ($generator, x64, Release)" @(
    'cmake', '-S', $Source, '-B', $buildDir, '-G', $generator,
    '-DCMAKE_BUILD_TYPE=Release',
    "-DBUILD_SHARED_LIBS=$shared",
    "-DCMAKE_INSTALL_PREFIX=$Prefix"
)

# No --config: these are single-config generators, where the build type was
# fixed at configure time and --config is silently ignored.
Invoke-Step 'Building'   @('cmake', '--build',   $buildDir)
Invoke-Step 'Installing' @('cmake', '--install', $buildDir)

# --- verify -----------------------------------------------------------------

Write-Host ""
Write-Host "==> Verifying" -ForegroundColor Cyan

$lib = Join-Path $Prefix 'lib\uiohook.lib'
$hdr = Join-Path $Prefix 'include\uiohook.h'
$dll = Join-Path $Prefix 'bin\uiohook.dll'

$missing = @($hdr, $lib | Where-Object { -not (Test-Path $_) })
if (-not $Static -and -not (Test-Path $dll)) { $missing += $dll }
if ($missing.Count -gt 0) {
    throw "Install finished but these are missing:`n  $($missing -join "`n  ")"
}

Write-Host "    $hdr" -ForegroundColor DarkGray
Write-Host "    $lib" -ForegroundColor DarkGray
if (-not $Static) { Write-Host "    $dll" -ForegroundColor DarkGray }

if (Get-Command 'dumpbin' -ErrorAction SilentlyContinue) {
    $match = & dumpbin /headers $lib |
             Select-String -Pattern 'machine \(' |
             Select-Object -First 1

    if ($null -eq $match) {
        Write-Warning "dumpbin produced no 'machine (...)' line, so the architecture could not be confirmed. schnellerTyp-e's CMake will check it at configure time regardless."
    } elseif ($match.Line -match 'x64') {
        Write-Host "    $($match.Line.Trim())" -ForegroundColor Green
    } else {
        throw "Built library reports '$($match.Line.Trim())' rather than x64. Open a Developer PowerShell for VS and re-run."
    }
} else {
    Write-Warning "dumpbin not on PATH, so the architecture could not be confirmed here. schnellerTyp-e's CMake will check it at configure time regardless."
}

# The architecture check above is not enough on its own. A Debug build is the
# right architecture and still unusable for distribution: it imports the debug C
# runtime, which Microsoft does not redistribute, so the resulting uiohook.dll
# works on this machine and fails on every machine without Visual Studio with
# "ucrtbased.dll was not found". This project shipped exactly that once, so the
# DLL is checked here as well as at packaging time.

if (-not $Static -and (Test-Path $dll)) {
    $readImports = Join-Path $PSScriptRoot 'Get-PEImports.ps1'
    if (Test-Path $readImports) {
        $debugRuntimes = @(
            'vcruntime140d.dll', 'vcruntime140_1d.dll',
            'msvcp140d.dll', 'msvcp140_1d.dll', 'msvcp140_2d.dll',
            'ucrtbased.dll'
        )
        $needs = & $readImports -Path $dll |
                 Where-Object { $debugRuntimes -contains $_.ToLowerInvariant() }
        if ($needs) {
            throw @"
The installed uiohook.dll is a DEBUG build: it imports $($needs -join ', ').

That runtime is not redistributable, so this DLL cannot be shipped — it would
work here and fail on any machine without Visual Studio.

Delete $Prefix and run this script again. If it recurs, something else is
writing to that prefix; check for a Debug build of libuiohook configured
elsewhere.
"@
        }
        Write-Host "    release runtime (no debug CRT imports)" -ForegroundColor Green
    }
}

# --- what to do next --------------------------------------------------------

Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host ""
Write-Host "In Qt Creator: Projects -> Build -> CMake variables, set" -ForegroundColor White
Write-Host "    UIOHOOK_ROOT = $($Prefix -replace '\\','/')" -ForegroundColor Yellow
Write-Host "then Run CMake and rebuild. Use forward slashes: CMake treats \ as an escape." -ForegroundColor White
Write-Host ""
Write-Host "If the old x86 build is still in Qt Creator's cache, delete the build" -ForegroundColor White
Write-Host "directory or use Build -> Clear CMake Configuration first." -ForegroundColor White
