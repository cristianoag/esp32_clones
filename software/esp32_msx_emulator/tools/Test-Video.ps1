$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$root = Join-Path ([IO.Path]::GetTempPath()) ("msx-video-" + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$include = Join-Path $root 'VideoUnderTest.inc'
$output = Join-Path $root 'video.exe'
try {
    $source = [IO.File]::ReadAllText((Join-Path $project 'src\main.cpp'))
    $start = $source.IndexOf('void msxPresent(', [StringComparison]::Ordinal)
    $end = $source.IndexOf('void msxPollKeyboard(', [StringComparison]::Ordinal)
    if ($start -lt 0 -or $end -le $start) { throw 'Cannot locate production video callback.' }
    [IO.File]::WriteAllText($include, $source.Substring($start, $end - $start), [Text.Encoding]::ASCII)
    & g++ -O2 -std=c++11 -Wall -Wextra -Werror "-I$root" "-I$project\src" "$project\tests\VideoTests.cpp" -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Video test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Video regression failed.' }
}
finally {
    foreach ($file in @($include,$output)) { if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file } }
    Remove-Item -LiteralPath $root
}
