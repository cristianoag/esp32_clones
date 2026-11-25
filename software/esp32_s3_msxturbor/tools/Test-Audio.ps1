$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$output = Join-Path ([IO.Path]::GetTempPath()) ('a1st-audio-' + [Guid]::NewGuid().ToString('N') + '.exe')
try {
    & $compiler -std=c++11 -O2 -Wall -Wextra -Werror "-I$(Join-Path $project 'tests\audio_stubs')" `
        "-I$(Join-Path $project 'src')" (Join-Path $project 'src\MsxAudio.cpp') `
        (Join-Path $project 'tests\AudioOutputTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Audio output test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Audio output regression failed.' }
}
finally {
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
