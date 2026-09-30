param(
    [string]$IIPlusRom,
    [string]$IIeRom,
    [string]$DiskRom,
    [string]$KaratekaDisk
)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
if (-not $IIPlusRom) { $IIPlusRom = Join-Path $project 'sdcard\apple2\bios\apple2plus.rom' }
if (-not $IIeRom) { $IIeRom = Join-Path $project 'sdcard\apple2\bios\apple2e.rom' }
if (-not $DiskRom) { $DiskRom = Join-Path $project 'sdcard\apple2\bios\disk2.rom' }
foreach ($path in @($IIPlusRom, $IIeRom, $DiskRom)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Supply ROMs before running smoke tests: $path" }
}
if ($KaratekaDisk) {
    $expected = 'FD347AF286DC852FF276FBAA85470E410ABD810653F71B948BC884D6929679C6'
    if ((Get-FileHash -LiteralPath $KaratekaDisk -Algorithm SHA256).Hash -ne $expected) {
        throw 'The optional Karateka regression expects the reported 35-track NIB revision; its SHA256 does not match.'
    }
}
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$output = Join-Path $project ("tests\rom-smoke-" + [Guid]::NewGuid().ToString('N') + '.exe')
$oldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
    & $compiler -std=c++11 -O2 -Wall -Wextra -Werror `
        "-I$(Join-Path $project 'src')" "-I$(Join-Path $project 'lib\chips')" `
        (Join-Path $project 'src\AppleCore.cpp') (Join-Path $project 'src\AppleVideo.cpp') `
        (Join-Path $project 'tests\RomSmoke.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'ROM smoke test compilation failed.' }
    $romArguments = @($IIPlusRom, $IIeRom, $DiskRom)
    if ($KaratekaDisk) { $romArguments += $KaratekaDisk }
    & $output @romArguments
    if ($LASTEXITCODE -ne 0) { throw 'ROM smoke test failed.' }
} finally {
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
