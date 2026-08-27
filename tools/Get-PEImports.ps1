<#
.SYNOPSIS
    Read the import table of a Windows PE file (.exe / .dll).

.DESCRIPTION
    Answers one question the packaging script has to be able to ask: which DLLs
    does this binary actually require?

    The reason it exists: a Debug binary imports the *debug* C runtime —
    VCRUNTIME140D.dll, ucrtbased.dll, MSVCP140D.dll. Those are not
    redistributable and exist only on machines with Visual Studio installed. A
    package containing one works perfectly on the developer's machine and fails
    on every other machine with "the code execution cannot proceed because
    ucrtbased.dll was not found" — which is precisely the failure this project
    shipped to a user once. A build machine cannot detect it by running the
    program, because the build machine is the one machine where it works.

    Deliberately not `dumpbin /dependents`: dumpbin only exists inside a Visual
    Studio developer shell and its output is a human-readable format that is not
    contractual. Parsing the PE header is a dozen lines and depends on nothing.

.PARAMETER Path
    The .exe or .dll to inspect.

.EXAMPLE
    .\tools\Get-PEImports.ps1 -Path .\uiohook.dll
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory, ValueFromPipeline, Position = 0)]
    [string]$Path
)

process {
    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path $Path))

    # DOS header -> PE header offset at 0x3C.
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
    if ($bytes[$peOffset]     -ne 0x50 -or $bytes[$peOffset + 1] -ne 0x45 -or
        $bytes[$peOffset + 2] -ne 0x00 -or $bytes[$peOffset + 3] -ne 0x00) {
        throw "$Path is not a PE image (no 'PE\0\0' signature)."
    }

    $sectionCount = [BitConverter]::ToUInt16($bytes, $peOffset + 6)
    $optionalSize = [BitConverter]::ToUInt16($bytes, $peOffset + 20)
    $optional     = $peOffset + 24

    # PE32 (0x10b) and PE32+ (0x20b) put the data directories at different
    # offsets, because PE32+ widens four fields to 64 bits.
    $magic     = [BitConverter]::ToUInt16($bytes, $optional)
    $dirOffset = $optional + $(if ($magic -eq 0x20b) { 112 } else { 96 })

    # Data directory entry 1 is the import table.
    $importRva = [BitConverter]::ToUInt32($bytes, $dirOffset + 8)
    if ($importRva -eq 0) { return }   # genuinely imports nothing

    # Section table follows the optional header; needed to map RVAs to file
    # offsets, which differ because sections are aligned differently on disk
    # and in memory.
    $sections = @()
    $walk = $optional + $optionalSize
    for ($i = 0; $i -lt $sectionCount; $i++) {
        $sections += [pscustomobject]@{
            VirtualSize    = [BitConverter]::ToUInt32($bytes, $walk + 8)
            VirtualAddress = [BitConverter]::ToUInt32($bytes, $walk + 12)
            RawSize        = [BitConverter]::ToUInt32($bytes, $walk + 16)
            RawAddress     = [BitConverter]::ToUInt32($bytes, $walk + 20)
        }
        $walk += 40
    }

    function ConvertTo-FileOffset([uint32]$rva) {
        foreach ($s in $sections) {
            $span = [Math]::Max($s.VirtualSize, $s.RawSize)
            if ($rva -ge $s.VirtualAddress -and $rva -lt $s.VirtualAddress + $span) {
                return $s.RawAddress + ($rva - $s.VirtualAddress)
            }
        }
        return -1
    }

    # Walk the import descriptors: 20 bytes each, name RVA at +12, terminated by
    # an all-zero descriptor.
    $entry = ConvertTo-FileOffset $importRva
    while ($entry -ge 0 -and $entry + 20 -le $bytes.Length) {
        $nameRva = [BitConverter]::ToUInt32($bytes, $entry + 12)
        if ($nameRva -eq 0) { break }

        $nameOffset = ConvertTo-FileOffset $nameRva
        if ($nameOffset -lt 0) { break }

        $end = $nameOffset
        while ($end -lt $bytes.Length -and $bytes[$end] -ne 0) { $end++ }
        [System.Text.Encoding]::ASCII.GetString($bytes, $nameOffset, $end - $nameOffset)

        $entry += 20
    }
}
