$ErrorActionPreference = 'Stop'
$root = Join-Path ([IO.Path]::GetTempPath()) ('apple-rom-import-' + [Guid]::NewGuid().ToString('N'))
$files = @()
try {
    [IO.Directory]::CreateDirectory($root) | Out-Null
    $plus = Join-Path $root 'plus.rom'
    $iie = Join-Path $root 'iie.rom'
    $disk = Join-Path $root 'disk.rom'
    $short = Join-Path $root 'short.rom'
    $files += @($plus, $iie, $disk, $short)
    [IO.File]::WriteAllBytes($plus, (New-Object byte[] 12288))
    [IO.File]::WriteAllBytes($iie, (New-Object byte[] 16384))
    [IO.File]::WriteAllBytes($disk, (New-Object byte[] 256))
    [IO.File]::WriteAllBytes($short, (New-Object byte[] 255))
    $destination = Join-Path $root 'sdcard'
    $importer = Join-Path $PSScriptRoot 'Import-Roms.ps1'
    & $importer -Destination $destination -IIPlusRom $plus -IIeRom $iie -DiskRom $disk
    $bios = Join-Path $destination 'apple2\bios'
    $copies = @('apple2plus.rom', 'apple2e.rom', 'disk2.rom') | ForEach-Object { Join-Path $bios $_ }
    $files += $copies
    $sizes = @(12288, 16384, 256)
    for ($i = 0; $i -lt $copies.Count; ++$i) {
        if ((Get-Item -LiteralPath $copies[$i]).Length -ne $sizes[$i]) { throw 'Imported ROM size mismatch.' }
    }
    $rejected = $false
    try { & $importer -Destination $destination -IIPlusRom $plus -DiskRom $short }
    catch {
        if ($_.Exception.Message -notlike '*raw 256-byte ROM*') { throw }
        $rejected = $true
    }
    if (-not $rejected -or (Get-Item -LiteralPath $copies[2]).Length -ne 256) {
        throw 'Invalid input was accepted or replaced the existing Disk II ROM.'
    }
    $rejected = $false
    try { & $importer -Destination $destination -DiskRom $disk }
    catch {
        if ($_.Exception.Message -notlike 'Supply at least one machine ROM*') { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw 'Missing machine ROM was accepted.' }
    Write-Output 'ROM import tests passed (synthetic files only).'
} finally {
    foreach ($file in $files) {
        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file }
    }
    foreach ($relative in @('sdcard\apple2\bios','sdcard\apple2','sdcard','')) {
        $directory = if ($relative) { Join-Path $root $relative } else { $root }
        if (Test-Path -LiteralPath $directory) { Remove-Item -LiteralPath $directory }
    }
}
