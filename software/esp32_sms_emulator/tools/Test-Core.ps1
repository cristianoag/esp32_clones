$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$cc = (Get-Command gcc -ErrorAction Stop).Source
$cxx = (Get-Command g++ -ErrorAction Stop).Source
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('sms-core-' + [Guid]::NewGuid().ToString('N'))
$oldPath = $env:PATH
$outputs = @()
try {
    New-Item -ItemType Directory -Path $temporary | Out-Null
    $env:PATH = (Split-Path -Parent $cxx) + ';' + $env:PATH
    $core = Join-Path $project 'lib\TotalSMS\src'
    $flags = @('-O2', '-DSMS_PIXEL_WIDTH=8', '-DSMS_FAST_AUDIO=1', '-DSN76489_CXX=0', "-I$core")
    foreach ($source in Get-ChildItem -LiteralPath $core -Filter '*.c' -Recurse) {
        $object = Join-Path $temporary ($source.BaseName + '.o')
        $outputs += $object
        & $cc -std=c99 @flags -c $source.FullName -o $object
        if ($LASTEXITCODE -ne 0) { throw "Core compilation failed: $($source.Name)" }
    }
    $executable = Join-Path $temporary 'sms-tests.exe'
    & $cxx -std=c++11 -Wall -Wextra -Werror @flags "-I$(Join-Path $project 'src')" `
        (Join-Path $project 'src\SmsCore.cpp') (Join-Path $project 'tests\CoreTests.cpp') @outputs -o $executable
    $outputs += $executable
    if ($LASTEXITCODE -ne 0) { throw 'SMS host test compilation failed.' }
    & $executable
    if ($LASTEXITCODE -ne 0) { throw 'SMS host regression failed.' }
    $menu = Join-Path $temporary 'sms-menu-tests.exe'
    $outputs += $menu
    & $cxx -std=c++11 -Wall -Wextra -Werror @flags `
        "-I$(Join-Path $project 'tests\menu_stubs')" "-I$(Join-Path $project 'src')" `
        (Join-Path $project 'src\SmsCore.cpp') (Join-Path $project 'src\SmsJoystickMapping.cpp') `
        (Join-Path $project 'tests\MenuTests.cpp') @($outputs | Where-Object { $_.EndsWith('.o') }) -o $menu
    if ($LASTEXITCODE -ne 0) { throw 'SMS menu test compilation failed.' }
    & $menu
    if ($LASTEXITCODE -ne 0) { throw 'SMS menu regression failed.' }
}
finally {
    $env:PATH = $oldPath
    foreach ($file in $outputs) {
        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file }
    }
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
}
