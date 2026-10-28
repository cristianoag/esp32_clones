$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$root = Join-Path ([IO.Path]::GetTempPath()) ('a1st-sound-menu-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
try {
    $source = [IO.File]::ReadAllText((Join-Path $project 'src\main.cpp'))
    $code = foreach ($section in @(
        @('bool applySoundSettings()', 'bool coldBoot()'),
        @('void soundMenu()', 'void present();')
    )) {
        $start = $source.IndexOf($section[0], [StringComparison]::Ordinal)
        $end = $source.IndexOf($section[1], [StringComparison]::Ordinal)
        if ($start -lt 0 -or $end -le $start) { throw "Cannot locate sound section: $($section[0])" }
        $source.Substring($start, $end - $start)
    }
    [IO.File]::WriteAllText((Join-Path $root 'SoundMenu.inc'), ($code -join "`n"))
    $output = Join-Path $root 'sound.exe'
    & $compiler -std=c++11 -O2 -Wall -Wextra -Werror "-I$root" "-I$(Join-Path $project 'src')" `
        "-I$(Join-Path $project 'lib\bluemsx\include')" "-I$(Join-Path $project 'tests\audio_stubs')" `
        (Join-Path $project 'tests\SoundMenuTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Sound menu test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Sound menu regression failed.' }
}
finally {
    $env:PATH = $oldPath
    foreach ($name in @('SoundMenu.inc', 'sound.exe')) {
        $path = Join-Path $root $name
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    Remove-Item -LiteralPath $root
}
