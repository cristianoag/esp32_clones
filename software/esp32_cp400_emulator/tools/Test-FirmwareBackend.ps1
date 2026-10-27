$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$output = Join-Path ([IO.Path]::GetTempPath()) ('cp400-backend-' + [Guid]::NewGuid().ToString('N') + '.exe')
try {
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$(Join-Path $project 'tests\updater_stubs')" `
        "-I$(Join-Path $project 'src')" (Join-Path $project 'src\FirmwareUpdater.cpp') `
        (Join-Path $project 'src\Cp400Flh.cpp') (Join-Path $project 'tests\FirmwareBackendTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'CP400 OTA backend test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'CP400 OTA backend regression failed.' }
}
finally {
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
