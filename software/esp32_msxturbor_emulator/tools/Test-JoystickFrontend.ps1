$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$root = Join-Path ([IO.Path]::GetTempPath()) ('a1st-joystick-frontend-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
try {
    $source = [IO.File]::ReadAllText((Join-Path $project 'src\main.cpp'))
    $code = foreach ($section in @(
        @('bool saveSettings()', 'bool chooseFile('),
        @('void firmwareUpdate()', 'const char* baseName(')
    )) {
        $start = $source.IndexOf($section[0], [StringComparison]::Ordinal)
        $end = $source.IndexOf($section[1], [StringComparison]::Ordinal)
        if ($start -lt 0 -or $end -le $start) { throw "Cannot locate frontend section: $($section[0])" }
        $source.Substring($start, $end - $start)
    }
    $start = $source.IndexOf('void loop()', [StringComparison]::Ordinal)
    if ($start -lt 0) { throw 'Cannot locate frontend loop.' }
    $code += $source.Substring($start)
    [IO.File]::WriteAllText((Join-Path $root 'JoystickFrontend.inc'), ($code -join "`n"))
    $output = Join-Path $root 'frontend.exe'
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$root" `
        (Join-Path $project 'tests\JoystickFrontendTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Joystick frontend test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Joystick frontend regression failed.' }
}
finally {
    $env:PATH = $oldPath
    foreach ($name in @('JoystickFrontend.inc', 'frontend.exe')) {
        $path = Join-Path $root $name
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    Remove-Item -LiteralPath $root
}
