$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$root = Join-Path ([IO.Path]::GetTempPath()) ('msx-file-diagnostics-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
try {
    $core = [IO.File]::ReadAllText((Join-Path $project 'src\MsxCore.cpp'))
    $code = foreach ($section in @(
        @('static bool fileFailure(', 'static bool validateCartridge('),
        @('static bool mediaResult(', 'bool MsxInspectCartridge('),
        @('static bool validateMedia(const char* path, bool tape', 'bool MsxDiskAvailable()')
    )) {
        $start = $core.IndexOf($section[0], [StringComparison]::Ordinal)
        $end = $core.IndexOf($section[1], [StringComparison]::Ordinal)
        if ($start -lt 0 -or $end -le $start) { throw "Cannot locate file validation section: $($section[0])" }
        $core.Substring($start, $end - $start)
    }
    [IO.File]::WriteAllText((Join-Path $root 'FileValidationUnderTest.inc'), ($code -join "`n"))
    $source = [IO.File]::ReadAllText((Join-Path $project 'src\main.cpp'))
    $start = $source.IndexOf('static void reportMediaFileError(', [StringComparison]::Ordinal)
    $end = $source.IndexOf('static bool saveSettings()', [StringComparison]::Ordinal)
    if ($start -lt 0 -or $end -le $start) { throw 'Cannot locate frontend file-validation section.' }
    [IO.File]::WriteAllText((Join-Path $root 'FilePreflightUnderTest.inc'), $source.Substring($start, $end - $start))
    $output = Join-Path $root 'diagnostics.exe'
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$root" "-I$(Join-Path $project 'src')" `
        (Join-Path $project 'tests\FileDiagnosticsTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'MSX file diagnostics test compilation failed.' }
    Push-Location $root
    try {
        & $output
        if ($LASTEXITCODE -ne 0) { throw 'MSX file diagnostics regression failed.' }
    }
    finally { Pop-Location }
}
finally {
    $env:PATH = $oldPath
    foreach ($name in @('FileValidationUnderTest.inc', 'FilePreflightUnderTest.inc', 'diagnostics.exe',
                         'valid.rom', 'bad-size.rom', 'bad-header.rom', 'valid.dsk', 'bad-size.dsk',
                         'valid.cas', 'bad-marker.cas', 'short.cas')) {
        $path = Join-Path $root $name
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    Remove-Item -LiteralPath $root
}
