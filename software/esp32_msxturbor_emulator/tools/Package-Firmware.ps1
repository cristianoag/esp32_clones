param([string]$Version = '1.01')
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d\.\d{2}$') { throw 'Version must use the 1.00 format.' }
$project = Split-Path -Parent $PSScriptRoot
$source = Join-Path $project '.pio\build\esp32-s3-devkitc-1-n16r8\firmware.bin'
$image = [IO.File]::ReadAllBytes($source)
if ($image.Length -lt 24 -or $image[0] -ne 0xe9 -or
    $image[12] -ne 9 -or $image[13] -ne 0 -or $image.Length -gt 0x400000) {
    throw 'Invalid or oversized ESP32-S3 application image.'
}
[uint64]$checksum = 45 + 126
foreach ($value in $image) { $checksum += $value }
$header = [Text.Encoding]::ASCII.GetBytes("$checksum-~")
$package = [byte[]]::new($header.Length + $image.Length)
[Array]::Copy($header, 0, $package, 0, $header.Length)
[Array]::Copy($image, 0, $package, $header.Length, $image.Length)
$directory = Join-Path $project 'dist'
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$path = Join-Path $directory "ESP32_MSXTURBOR-$Version.FLH"
[IO.File]::WriteAllBytes($path, $package)
& (Join-Path $PSScriptRoot 'Test-FirmwareUpdate.ps1') -PackagePaths @($path)
Write-Host "Created and verified $path ($($package.Length) bytes)"
