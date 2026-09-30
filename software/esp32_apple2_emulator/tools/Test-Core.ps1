$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$output = Join-Path $project ("tests\apple-tests-" + [Guid]::NewGuid().ToString('N') + '.exe')
$oldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
    & $compiler -std=c++11 -O2 -Wall -Wextra -Werror `
        "-I$(Join-Path $project 'src')" "-I$(Join-Path $project 'lib\chips')" `
        (Join-Path $project 'src\AppleCore.cpp') (Join-Path $project 'src\AppleVideo.cpp') `
        (Join-Path $project 'tests\AppleTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Apple regression compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Apple regression failed.' }
} finally {
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
