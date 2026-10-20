$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$output = Join-Path ([System.IO.Path]::GetTempPath()) ("sms-joystick-runtime-" + [Guid]::NewGuid().ToString('N') + '.exe')
$oldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$(Join-Path $project 'tests\joystick_stubs')" `
        "-I$(Join-Path $project 'src')" (Join-Path $project 'src\SmsJoysticks.cpp') `
        (Join-Path $project 'src\SmsJoystickMapping.cpp') (Join-Path $project 'tests\JoystickRuntimeTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Joystick runtime test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Joystick runtime regression failed.' }
}
finally {
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
