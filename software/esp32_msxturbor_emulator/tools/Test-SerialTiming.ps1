$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$root = Join-Path ([IO.Path]::GetTempPath()) ('a1st-serial-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
try {
    $source = [IO.File]::ReadAllText((Join-Path $project 'src\main.cpp'))
    $code = foreach ($signature in @('void pace(', 'bool startBufferedSerial()')) {
        $start = $source.IndexOf($signature, [StringComparison]::Ordinal)
        if ($start -lt 0) { throw "Cannot locate $signature" }
        $tail = $source.Substring($start)
        $end = [regex]::Match($tail, '(?m)^}')
        if (!$end.Success) { throw "Cannot locate function end: $signature" }
        $tail.Substring(0, $end.Index + 1)
    }
    [IO.File]::WriteAllText((Join-Path $root 'SerialTiming.inc'), ($code -join "`n"))
    $output = Join-Path $root 'serial.exe'
    & $compiler -std=c++11 -O2 -Wall -Wextra -Werror "-I$root" "-I$(Join-Path $project 'src')" `
        "-I$(Join-Path $project 'lib\bluemsx\include')" `
        (Join-Path $project 'tests\SerialTimingTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Serial timing test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Serial timing regression failed.' }
}
finally {
    $env:PATH = $oldPath
    foreach ($name in @('SerialTiming.inc', 'serial.exe')) {
        $path = Join-Path $root $name
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    Remove-Item -LiteralPath $root
}
