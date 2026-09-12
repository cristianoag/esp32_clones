$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$root = Join-Path ([System.IO.Path]::GetTempPath()) ("msx-menu-test-" + [Guid]::NewGuid().ToString('N'))
$include = Join-Path $root 'MediaMenuUnderTest.inc'
$output = Join-Path $root 'menu.exe'
[System.IO.Directory]::CreateDirectory($root) | Out-Null
try {
    # Exercise the actual menu functions with scripted keys and hardware stubs.
    $source = [System.IO.File]::ReadAllText((Join-Path $project 'src\main.cpp'))
    $sections = @(
        @('static bool saveSettings()', 'static void readSettings()'),
        @('static void selectCartridge(', 'static void drawProgressGauge(')
    )
    $code = foreach ($section in $sections) {
        $start = $source.IndexOf($section[0], [StringComparison]::Ordinal)
        $end = $source.IndexOf($section[1], [StringComparison]::Ordinal)
        if ($start -lt 0 -or $end -le $start) { throw "Cannot locate production menu section: $($section[0])" }
        $source.Substring($start, $end - $start)
    }
    [System.IO.File]::WriteAllText($include, ($code -join "`n"), [System.Text.Encoding]::ASCII)
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$root" "-I$(Join-Path $project 'src')" `
        (Join-Path $project 'tests\MediaMenuTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Media menu test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Media menu regression failed.' }
}
finally {
    foreach ($path in @($output, $include)) {
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    Remove-Item -LiteralPath $root
}
