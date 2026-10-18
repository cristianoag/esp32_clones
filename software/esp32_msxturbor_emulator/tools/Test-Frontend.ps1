$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$build = Join-Path $project 'tests\build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$executable = Join-Path $build 'frontend-tests.exe'
& g++ '-std=c++11' '-Wall' '-Wextra' '-Werror' "-I$(Join-Path $project 'src')" `
    (Join-Path $project 'src\MsxSettings.cpp') (Join-Path $project 'tests\FrontendTests.cpp') '-o' $executable
if ($LASTEXITCODE -ne 0) { throw 'A1ST frontend tests did not compile.' }
& $executable
if ($LASTEXITCODE -ne 0) { throw 'A1ST frontend regressions failed.' }
