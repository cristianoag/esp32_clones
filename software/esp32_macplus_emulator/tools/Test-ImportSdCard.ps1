$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('macplus-import-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($temporary) | Out-Null
try {
    $rom = Join-Path $temporary 'generated-test.rom'
    $bytes = New-Object byte[] 131072
    $bytes[3] = 64
    $bytes[5] = 64
    [IO.File]::WriteAllBytes($rom, $bytes)
    $destination = Join-Path $temporary 'sdcard'
    & (Join-Path $PSScriptRoot 'Import-SdCard.ps1') -Destination $destination -Rom $rom -BlankDiskMiB 1
    $copied = Join-Path $destination 'macplus\bios\macplus.rom'
    if ((Get-FileHash -LiteralPath $rom).Hash -ne (Get-FileHash -LiteralPath $copied).Hash) {
        throw 'ROM copy differs.'
    }
    $disk = Join-Path $destination 'macplus\disks\hd.img'
    if ((Get-Item -LiteralPath $disk).Length -ne 1MB) { throw 'Wrong blank disk size.' }
    $stream = [IO.File]::OpenWrite($disk)
    try { $stream.WriteByte(90) } finally { $stream.Dispose() }
    $hash = (Get-FileHash -LiteralPath $disk).Hash
    & (Join-Path $PSScriptRoot 'Import-SdCard.ps1') -Destination $destination -Rom $rom
    if ((Get-FileHash -LiteralPath $disk).Hash -ne $hash) { throw 'Existing disk was overwritten.' }
    $bytes[10] = 1
    [IO.File]::WriteAllBytes($rom, $bytes)
    $rejected = $false
    try { & (Join-Path $PSScriptRoot 'Import-SdCard.ps1') -Destination $destination -Rom $rom }
    catch { $rejected = $true }
    if (-not $rejected) { throw 'Invalid ROM was accepted.' }
    foreach ($path in @('sdcard\macplus\bios\macplus.rom', 'sdcard\macplus\disks\hd.img', 'elsewhere.ROM', 'elsewhere.IMG')) {
        & git -C $project check-ignore --quiet --no-index -- $path
        if ($LASTEXITCODE -ne 0) { throw "Not git-excluded: $path" }
    }
    Write-Output 'SD importer tests passed: checksum validation, copy, blank size, preservation and Git exclusions.'
}
finally {
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
