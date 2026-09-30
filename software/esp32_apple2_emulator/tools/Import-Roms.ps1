param(
    [string]$Destination = (Join-Path (Split-Path -Parent $PSScriptRoot) 'sdcard'),
    [string]$IIPlusRom,
    [string]$IIeRom,
    [Parameter(Mandatory = $true)][string]$DiskRom
)
$ErrorActionPreference = 'Stop'
if (-not $IIPlusRom -and -not $IIeRom) { throw 'Supply at least one machine ROM with -IIPlusRom or -IIeRom.' }
$items = @(
    @{ Source = $IIPlusRom; Name = 'apple2plus.rom'; Size = 12288 },
    @{ Source = $IIeRom; Name = 'apple2e.rom'; Size = 16384 },
    @{ Source = $DiskRom; Name = 'disk2.rom'; Size = 256 }
) | Where-Object { $_.Source }
foreach ($item in $items) {
    $file = Get-Item -LiteralPath $item.Source -ErrorAction Stop
    if ($file.PSIsContainer -or $file.Length -ne $item.Size) {
        throw "$($item.Name) must be a raw $($item.Size)-byte ROM."
    }
}
$bios = Join-Path ([IO.Path]::GetFullPath($Destination)) 'apple2\bios'
[IO.Directory]::CreateDirectory($bios) | Out-Null
foreach ($item in $items) {
    $target = Join-Path $bios $item.Name
    if ([IO.Path]::GetFullPath($item.Source) -ne $target) {
        Copy-Item -LiteralPath $item.Source -Destination $target -Force -ErrorAction Stop
    }
    if ((Get-FileHash -LiteralPath $item.Source -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash) {
        throw "ROM copy verification failed: $target"
    }
    Write-Output "Verified $target"
}
Write-Output 'ROM sizes/copies verified, not machine identity. Use an original NMOS-6502 IIe ROM, not enhanced IIe.'
