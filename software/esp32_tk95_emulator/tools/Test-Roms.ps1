param(
    [string]$BiosDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'sdcard\tk\bios')
)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$output = Join-Path $project ("tests\tk-rom-smoke-" + [Guid]::NewGuid().ToString('N') + '.exe')
$oldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
    & $compiler -std=c++11 -O2 -Wall -Wextra -Werror `
        "-I$(Join-Path $project 'src')" "-I$(Join-Path $project 'lib\chips')" `
        (Join-Path $project 'src\TkCore.cpp') (Join-Path $project 'tests\RomSmoke.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'ROM smoke test compilation failed.' }
    foreach ($name in @('tk95.rom', 'tk90.rom')) {
        $model = if ($name -eq 'tk95.rom') { 'tk95' } else { 'tk90x' }
        & $output (Join-Path $BiosDirectory $name) $model
        if ($LASTEXITCODE -ne 0) { throw "ROM boot/keyboard/BASIC regression failed: $name" }
    }
} finally {
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
