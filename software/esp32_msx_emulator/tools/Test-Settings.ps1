$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$output = Join-Path ([System.IO.Path]::GetTempPath()) ("msx-settings-test-" + [Guid]::NewGuid().ToString('N') + '.exe')
try {
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$(Join-Path $project 'src')" `
        (Join-Path $project 'src\MsxSettings.cpp') (Join-Path $project 'tests\SettingsTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Settings test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Settings regression failed.' }
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$(Join-Path $project 'tests\browser_stubs')" `
        "-I$(Join-Path $project 'src')" (Join-Path $project 'src\MsxFileBrowser.cpp') `
        (Join-Path $project 'src\MsxSettings.cpp') (Join-Path $project 'tests\FileBrowserTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Browser test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Browser regression failed.' }
}
finally {
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
