[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][Alias('PanasonicDirectory')][string]$SourceDirectory,
    [Parameter(Mandatory = $true)][string]$Destination,
    [string]$FirmwareRom,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $SourceDirectory).ProviderPath
if (!$FirmwareRom) { $FirmwareRom = Join-Path $source 'fs-a1st_firmware.rom' }
$specifications = @(
    @{
        Source = $FirmwareRom
        Name = 'fs-a1st_firmware.rom'
        Size = 4194304
        Hashes = @('c212b11fda13f83dafed688c54d098e7e47ab225',
                   '4b84465c2faa802bfd5f772118c4d31f993f29d0')
    },
    @{
        Source = (Join-Path $source 'fs-a1st_kanjifont.rom')
        Name = 'fs-a1st_kanjifont.rom'
        Size = 262144
        Hashes = @('5aff2d9b6efc723bc395b0f96f0adfa83cc54a49')
    }
)
$output = [IO.Path]::GetFullPath((Join-Path $Destination 'msxturbor\bios\fs-a1st'))
foreach ($specification in $specifications) {
    $file = Get-Item -LiteralPath $specification.Source
    if ($file.PSIsContainer -or $file.Length -ne $specification.Size) {
        throw "Wrong size for $($specification.Name): expected $($specification.Size) bytes."
    }
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA1).Hash.ToLowerInvariant()
    if ($hash -notin $specification.Hashes) {
        throw "Unrecognized A1ST ROM SHA-1 for $($specification.Name): $hash"
    }
    $specification.Hash = $hash
    $specification.Output = Join-Path $output $specification.Name
    if (Test-Path -LiteralPath $specification.Output) {
        $existing = (Get-FileHash -LiteralPath $specification.Output -Algorithm SHA1).Hash.ToLowerInvariant()
        if ($existing -ne $hash -and !$Force) {
            throw "Destination differs: $($specification.Output). Use -Force only to replace it intentionally."
        }
    }
}
New-Item -ItemType Directory -Path $output -Force | Out-Null
foreach ($specification in $specifications) {
    if ([IO.Path]::GetFullPath($specification.Source) -ne $specification.Output) {
        [IO.File]::Copy($specification.Source, $specification.Output, $true)
    }
    $verified = (Get-FileHash -LiteralPath $specification.Output -Algorithm SHA1).Hash.ToLowerInvariant()
    if ($verified -ne $specification.Hash) { throw "ROM changed during import: $($specification.Name)" }
    Write-Host "Verified $($specification.Name): $verified"
}
New-Item -ItemType Directory -Path (Join-Path $Destination 'msxturbor\disks') -Force | Out-Null
Write-Host "A1ST SD tree ready at $output. No BIOS bytes were patched or extracted."
