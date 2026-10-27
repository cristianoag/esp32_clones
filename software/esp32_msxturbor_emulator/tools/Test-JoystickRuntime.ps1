$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$output = Join-Path ([System.IO.Path]::GetTempPath()) ("msx-joystick-runtime-" + [Guid]::NewGuid().ToString('N') + '.exe')
try {
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$(Join-Path $project 'tests\joystick_stubs')" `
        "-I$(Join-Path $project 'src')" (Join-Path $project 'src\MsxJoysticks.cpp') `
        (Join-Path $project 'src\MsxJoystickMapping.cpp') (Join-Path $project 'tests\JoystickRuntimeTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Joystick runtime test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Joystick runtime regression failed.' }
}
finally {
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
