$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$root = Join-Path ([IO.Path]::GetTempPath()) ('a1st-video-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
try {
    $source = [IO.File]::ReadAllText((Join-Path $project 'src\main.cpp'))
    $match = [regex]::Match($source, 'void present\(\)\s*\{')
    $end = $source.IndexOf('void pace(', $match.Index, [StringComparison]::Ordinal)
    if (!$match.Success -or $end -le $match.Index) { throw 'Cannot locate frontend presentation.' }
    [IO.File]::WriteAllText((Join-Path $root 'VideoPresent.inc'), $source.Substring($match.Index, $end - $match.Index))
    $output = Join-Path $root 'video.exe'
    & $compiler -std=c++11 -O2 -Wall -Wextra -Werror "-I$root" "-I$(Join-Path $project 'src')" `
        "-I$(Join-Path $project 'lib\bluemsx\include')" `
        (Join-Path $project 'tests\VideoPresentTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Video presentation test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Video presentation regression failed.' }
}
finally {
    $env:PATH = $oldPath
    foreach ($name in @('VideoPresent.inc', 'video.exe')) {
        $path = Join-Path $root $name
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    Remove-Item -LiteralPath $root
}
