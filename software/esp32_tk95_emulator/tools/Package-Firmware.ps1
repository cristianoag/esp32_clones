param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Output,
    [switch]$VerifyOnly
)
$ErrorActionPreference = 'Stop'
$sourcePath = [IO.Path]::GetFullPath($Source)
$outputPath = [IO.Path]::GetFullPath($Output)
if ($sourcePath -eq $outputPath) { throw 'Source and output must be different files.' }
$image = [IO.File]::ReadAllBytes($sourcePath)
if ($image.Length -lt 24 -or $image.Length -gt 0x400000 -or $image[0] -ne 0xe9 -or
    $image[12] -ne 9 -or $image[13] -ne 0) { throw 'Expected an ESP32-S3 application image of at most 4 MiB.' }
[uint64]$sum = 45 + 126
foreach ($byte in $image) { $sum += $byte }
$header = [Text.Encoding]::ASCII.GetBytes("$sum-~")
if (-not $VerifyOnly) {
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($outputPath)) | Out-Null
    $stream = [IO.File]::Create($outputPath)
    try {
        $stream.Write($header, 0, $header.Length)
        $stream.Write($image, 0, $image.Length)
        $stream.Flush($true)
    } finally { $stream.Dispose() }
}
$package = [IO.File]::ReadAllBytes($outputPath)
if ($package.Length -ne $header.Length + $image.Length) { throw 'FLH package length mismatch.' }
for ($index = 0; $index -lt $package.Length; ++$index) {
    $expected = if ($index -lt $header.Length) { $header[$index] } else { $image[$index - $header.Length] }
    if ($package[$index] -ne $expected) { throw "FLH checksum/header/payload mismatch at byte $index." }
}
Write-Output "Verified $outputPath ($($package.Length) bytes, checksum $sum)."
