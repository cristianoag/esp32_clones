$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$output = Join-Path $project ("tests\tk-tests-" + [Guid]::NewGuid().ToString('N') + '.exe')
$oldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
    & $compiler -std=c++11 -O2 -Wall -Wextra -Werror `
        "-I$(Join-Path $project 'src')" "-I$(Join-Path $project 'lib\chips')" `
        (Join-Path $project 'src\TkCore.cpp') (Join-Path $project 'src\TkSnapshot.cpp') `
        (Join-Path $project 'tests\TkTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'TK regression compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'TK regression failed.' }
} finally {
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
