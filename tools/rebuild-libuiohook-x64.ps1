<#
.SYNOPSIS
    Build and install a 64-bit libuiohook for schnellerTyp-e.

.DESCRIPTION
    Clones libuiohook if needed, configures it with -A x64 into a fresh build
    directory, builds and installs Release, then verifies the architecture of
    the resulting import library.

    The -A x64 and the fresh build directory are the whole point. Without the
    former, the architecture depends on which developer prompt happens to be
    open and which generator CMake picks; a 32-bit libuiohook against a 64-bit
    schnellerTyp-e fails as four unresolved hook_* symbols with one easily
    missed LNK4272 warning above them. And a CMake build tree cannot change
    platform in place, so reusing an existing one silently keeps the old
    architecture.

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
    Run from a "Developer PowerShell for VS" so that the MSVC toolchain and
    dumpbin are on PATH.
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
        throw "$tool is not on PATH. Open a 'Developer PowerShell for VS' and try again."
    }
}

if (-not (Get-Command 'cl' -ErrorAction SilentlyContinue)) {
    Write-Warning "cl.exe is not on PATH. This is probably a plain PowerShell rather than a Developer PowerShell for VS. Continuing, but CMake may not find a compiler."
}

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

Invoke-Step 'Configuring (x64)' @(
    'cmake', '-S', $Source, '-B', $buildDir, '-A', 'x64',
    "-DBUILD_SHARED_LIBS=$shared",
    "-DCMAKE_INSTALL_PREFIX=$Prefix"
)

Invoke-Step 'Building'   @('cmake', '--build',   $buildDir, '--config', 'Release')
Invoke-Step 'Installing' @('cmake', '--install', $buildDir, '--config', 'Release')

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
