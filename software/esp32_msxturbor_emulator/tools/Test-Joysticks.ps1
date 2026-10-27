$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compilerCommand = Get-Command g++ -ErrorAction SilentlyContinue
if ($compilerCommand) {
    $compiler = $compilerCommand.Source
} elseif (Test-Path -LiteralPath 'C:\msys64\ucrt64\bin\g++.exe') {
    $compiler = 'C:\msys64\ucrt64\bin\g++.exe'
} else {
    throw 'g++ is required to run the native joystick tests.'
}
$output = Join-Path $project ("tests\msx-joystick-test-" + [Guid]::NewGuid().ToString('N') + '.exe')
$oldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$(Join-Path $project 'src')" `
        (Join-Path $project 'src\MsxJoystickMapping.cpp') `
        (Join-Path $project 'tests\JoystickMappingTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Joystick test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Joystick mapping regression failed.' }
}
finally {
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
