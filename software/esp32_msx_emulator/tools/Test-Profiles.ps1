$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$output = Join-Path ([System.IO.Path]::GetTempPath()) ("msx-profile-test-" + [Guid]::NewGuid().ToString('N') + '.exe')
try {
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$(Join-Path $project 'src')" `
        (Join-Path $project 'src\MsxProfileConfig.cpp') (Join-Path $project 'tests\ProfileConfigTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Profile test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Profile parser regression failed.' }
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$(Join-Path $project 'tests\profile_stubs')" `
        "-I$(Join-Path $project 'tests\joystick_stubs')" "-I$(Join-Path $project 'src')" `
        (Join-Path $project 'src\MsxProfileConfig.cpp') (Join-Path $project 'src\MsxProfiles.cpp') `
        (Join-Path $project 'tests\ProfileLoadingTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Profile loading test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Profile loading regression failed.' }
}
finally {
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
