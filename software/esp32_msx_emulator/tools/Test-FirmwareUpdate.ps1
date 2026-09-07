$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$command = Get-Command g++ -ErrorAction SilentlyContinue
if ($command) { $compiler = $command.Source }
elseif (Test-Path -LiteralPath 'C:\msys64\ucrt64\bin\g++.exe') {
    $compiler = 'C:\msys64\ucrt64\bin\g++.exe'
}
else { throw 'Native g++ was not found (PATH or C:\msys64\ucrt64\bin).' }
$output = Join-Path $project ('tests\firmware-update-test-' + [Guid]::NewGuid().ToString('N') + '.exe')
$oldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$(Join-Path $project 'src')" `
        (Join-Path $project 'src\MsxFlh.cpp') (Join-Path $project 'tests\FirmwareUpdateTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Firmware update test compilation failed.' }
    $packages = @(Get-ChildItem -LiteralPath (Join-Path $project 'dist') -Filter 'ESP32_MSX-*.FLH' `
        -File -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })
    & $output @packages
    if ($LASTEXITCODE -ne 0) { throw 'Firmware update regression failed.' }
}
finally {
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
