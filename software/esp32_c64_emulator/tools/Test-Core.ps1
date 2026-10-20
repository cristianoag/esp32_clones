param([string]$RomDirectory = '', [switch]$RomSmoke)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$output = Join-Path $project ('tests\c64-tests-' + [Guid]::NewGuid().ToString('N') + '.exe')
$oldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
    $sources = @('src\C64Core.cpp', 'src\C64Media.cpp', 'lib\retroelec\CPU6502.cpp',
                 'lib\retroelec\CIA.cpp', 'lib\retroelec\VIC.cpp', 'lib\retroelec\SID.cpp',
                 'tests\C64Tests.cpp') | ForEach-Object { Join-Path $project $_ }
    & $compiler -std=c++11 -O2 -Wall -Wextra -Werror `
        "-I$(Join-Path $project 'src')" "-I$(Join-Path $project 'lib\retroelec')" @sources -o $output
    if ($LASTEXITCODE -ne 0) { throw 'C64 regression compilation failed.' }
    if ($RomSmoke) {
        if (-not $RomDirectory) { $RomDirectory = Join-Path $project 'sdcard\c64\bios' }
        & $output ([IO.Path]::GetFullPath($RomDirectory))
    } else { & $output }
    if ($LASTEXITCODE -ne 0) { throw 'C64 regression failed.' }
} finally {
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
