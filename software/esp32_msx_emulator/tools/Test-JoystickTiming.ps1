$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$output = Join-Path ([System.IO.Path]::GetTempPath()) ("msx-joystick-timing-" + [Guid]::NewGuid().ToString('N') + '.exe')
try {
    & $compiler -std=c++11 -Wall -Wextra -Werror (Join-Path $project 'tests\JoystickTimingTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Joystick timing test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Joystick timing regression failed.' }
    & $compiler -std=c++11 -Wall -Wextra -Werror (Join-Path $project 'tests\JoystickPacketTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Joystick packet test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Joystick packet regression failed.' }
}
finally {
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
