$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$output = Join-Path ([System.IO.Path]::GetTempPath()) ("msx-audio-test-" + [Guid]::NewGuid().ToString('N') + '.exe')
try {
    & $compiler -std=c++11 -O2 -Wall -Wextra -Werror "-I$(Join-Path $project 'tests\audio_stubs')" `
        "-I$(Join-Path $project 'src')" (Join-Path $project 'src\MsxAudio.cpp') `
        (Join-Path $project 'tests\AudioTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Audio test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Audio regression failed.' }
}
finally {
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
