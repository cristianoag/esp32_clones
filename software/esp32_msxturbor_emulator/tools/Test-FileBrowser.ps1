$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$root = Join-Path ([IO.Path]::GetTempPath()) ('a1st-browser-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
try {
    $source = [IO.File]::ReadAllText((Join-Path $project 'src\main.cpp'))
    $start = $source.IndexOf('bool chooseFile(', [StringComparison]::Ordinal)
    $end = $source.IndexOf('bool flushState()', [StringComparison]::Ordinal)
    if ($start -lt 0 -or $end -le $start) { throw 'Cannot locate frontend file browser.' }
    [IO.File]::WriteAllText((Join-Path $root 'FileBrowserFrontend.inc'), $source.Substring($start, $end - $start))
    $output = Join-Path $root 'browser.exe'
    & $compiler -std=c++11 -O2 -Wall -Wextra -Werror "-I$root" "-I$(Join-Path $project 'src')" `
        (Join-Path $project 'src\MsxSettings.cpp') (Join-Path $project 'tests\FileBrowserFrontendTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'File browser test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'File browser regression failed.' }
}
finally {
    $env:PATH = $oldPath
    foreach ($name in @('FileBrowserFrontend.inc', 'browser.exe')) {
        $path = Join-Path $root $name
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    Remove-Item -LiteralPath $root
}
