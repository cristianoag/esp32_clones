[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Rom,
    [Parameter(Mandatory = $true)][string]$Destination,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $Rom).ProviderPath
$bytes = [System.IO.File]::ReadAllBytes($source)
if ($bytes.Length -notin @(16384, 65536) -or $bytes[0] -ne 65 -or $bytes[1] -ne 66) {
    throw 'Expected a 16 KiB MSX-MUSIC or 64 KiB FM-PAC ROM with an AB header.'
}
$signature = [System.Text.Encoding]::ASCII.GetString($bytes, 0x18, 8)
if ($signature -cnotin @('APRLOPLL', 'PAC2OPLL')) {
    throw 'The ROM does not contain a supported MSX-MUSIC/FM-PAC identification signature.'
}
$directory = Join-Path ([System.IO.Path]::GetFullPath($Destination)) 'msx\audio'
$target = Join-Path $directory 'FMPAC.ROM'
if ([System.IO.File]::Exists($target) -and -not $Force) {
    throw "Audio BIOS already exists at '$target'; use -Force to replace it."
}
[System.IO.Directory]::CreateDirectory($directory) | Out-Null
[System.IO.File]::WriteAllBytes($target, $bytes)
$sha = [System.Security.Cryptography.SHA256]::Create()
try { $expected = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '') }
finally { $sha.Dispose() }
if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $expected) {
    throw 'Audio BIOS verification failed after writing.'
}
Write-Output "Imported $($bytes.Length)-byte $signature audio BIOS to $target."
Write-Output "SHA256: $expected"
