param([string]$PanasonicDirectory)
$ErrorActionPreference = 'Stop'
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('a1st-import-tests-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    $source = Join-Path $temporary 'source'
    $destination = Join-Path $temporary 'card'
    New-Item -ItemType Directory -Path $source | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $source 'fs-a1st_firmware.rom'), [byte[]]@(1,2,3))
    $failed = $false
    try { & (Join-Path $PSScriptRoot 'Import-Roms.ps1') -SourceDirectory $source -Destination $destination }
    catch { $failed = $true }
    if (!$failed -or (Test-Path -LiteralPath $destination)) { throw 'Invalid imports must fail before creating output.' }
    [IO.File]::WriteAllBytes((Join-Path $source 'fs-a1st_firmware.rom'), [byte[]]::new(4194304))
    $failed = $false
    try { & (Join-Path $PSScriptRoot 'Import-Roms.ps1') -SourceDirectory $source -Destination $destination }
    catch { $failed = $true }
    if (!$failed -or (Test-Path -LiteralPath $destination)) { throw 'Unknown hashes must not be imported.' }
    if ($PanasonicDirectory) {
        & (Join-Path $PSScriptRoot 'Import-Roms.ps1') -SourceDirectory $PanasonicDirectory -Destination $destination
        & (Join-Path $PSScriptRoot 'Import-Roms.ps1') -SourceDirectory $PanasonicDirectory -Destination $destination
        $imported = Join-Path $destination 'msxturbor\bios\fs-a1st\fs-a1st_firmware.rom'
        if ((Get-Item -LiteralPath $imported).Length -ne 4194304) { throw 'The complete 4 MiB dump must be preserved.' }
        [IO.File]::WriteAllBytes($imported, [byte[]]@(9))
        $failed = $false
        try { & (Join-Path $PSScriptRoot 'Import-Roms.ps1') -SourceDirectory $PanasonicDirectory -Destination $destination }
        catch { $failed = $true }
        if (!$failed) { throw 'Overwriting differing ROMs requires -Force.' }
    }
    Write-Host 'A1ST ROM import regressions passed.'
} finally {
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
