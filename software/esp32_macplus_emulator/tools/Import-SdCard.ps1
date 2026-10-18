param(
    [string]$Destination = (Join-Path (Split-Path -Parent $PSScriptRoot) 'sdcard'),
    [string]$Rom,
    [string]$Disk,
    [ValidateRange(1,1024)][int]$BlankDiskMiB = 100,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'

function Get-LocalFile([string]$Path) {
    if ($Path -match '^[a-zA-Z]+://') {
        throw 'Supply a local file you are authorized to use, not a download URL.'
    }
    $item = Get-Item -LiteralPath $Path -ErrorAction Stop
    if ($item.PSIsContainer) { throw "Expected a file: $Path" }
    return $item
}

function Assert-Disk($Item) {
    if ($Item.Length -lt 512 -or $Item.Length -gt 1GB -or $Item.Length % 512) {
        throw 'The SCSI image must be sector-aligned and between 512 bytes and 1 GiB.'
    }
}

function Copy-Verified($Source, [string]$Target) {
    if ($Source.FullName -ne $Target) {
        if ((Test-Path -LiteralPath $Target) -and -not $Force) {
            if ((Get-FileHash -LiteralPath $Source.FullName).Hash -ne
                (Get-FileHash -LiteralPath $Target).Hash) {
                throw "Refusing to overwrite $Target. Back it up, then explicitly use -Force."
            }
        } else {
            Copy-Item -LiteralPath $Source.FullName -Destination $Target -Force
        }
    }
    if ((Get-FileHash -LiteralPath $Source.FullName).Hash -ne
        (Get-FileHash -LiteralPath $Target).Hash) {
        throw "Copy verification failed: $Target"
    }
    Write-Output "Verified $Target"
}

$romSource = $null
$diskSource = $null
if ($Rom) {
    $romSource = Get-LocalFile $Rom
    if ($romSource.Length -ne 131072) { throw 'Expected a raw 128 KiB Macintosh Plus ROM.' }
    $bytes = [IO.File]::ReadAllBytes($romSource.FullName)
    [uint64]$sum = 0
    for ($i = 4; $i -lt $bytes.Length; $i += 2) {
        $sum += [uint64]$bytes[$i] * 256 + $bytes[$i + 1]
    }
    [uint64]$expected = [uint64]$bytes[0] * 16777216 + [uint64]$bytes[1] * 65536 +
                       [uint64]$bytes[2] * 256 + $bytes[3]
    if ($sum -ne $expected -or $bytes[4] -ne 0 -or $bytes[5] -ne 64) {
        throw 'Invalid Plus ROM checksum/reset vector; use an unmodified ROM.'
    }
}
if ($Disk) {
    $diskSource = Get-LocalFile $Disk
    Assert-Disk $diskSource
}

$root = [IO.Path]::GetFullPath($Destination)
$bios = Join-Path $root 'macplus\bios'
$disks = Join-Path $root 'macplus\disks'
[IO.Directory]::CreateDirectory($bios) | Out-Null
[IO.Directory]::CreateDirectory($disks) | Out-Null
if ($romSource) { Copy-Verified $romSource (Join-Path $bios 'macplus.rom') }
else { Write-Warning 'ROM not imported. Supply your authorized local copy with -Rom.' }

$targetDisk = Join-Path $disks 'hd.img'
if ($diskSource) {
    Copy-Verified $diskSource $targetDisk
} elseif (Test-Path -LiteralPath $targetDisk) {
    Assert-Disk (Get-Item -LiteralPath $targetDisk)
    Write-Output "Preserved existing disk: $targetDisk"
} else {
    $stream = [IO.File]::Open($targetDisk, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
    try {
        $stream.SetLength([long]$BlankDiskMiB * 1MB)
        $stream.Flush($true)
    } finally { $stream.Dispose() }
    if ((Get-Item -LiteralPath $targetDisk).Length -ne [long]$BlankDiskMiB * 1MB) {
        throw 'Blank SCSI disk length verification failed.'
    }
    Write-Output "Created $BlankDiskMiB MiB blank disk: $targetDisk"
    Write-Warning 'Blank disk is NOT bootable: install a partition map, SCSI driver and Macintosh system in another emulator.'
}
Write-Output 'The project sdcard folder, ROMs and IMG files are git-excluded. Never force-add Apple software.'
