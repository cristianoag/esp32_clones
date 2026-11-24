$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$root = Join-Path ([IO.Path]::GetTempPath()) ('cp400-browser-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$output = Join-Path $root 'browser.exe'
$include = Join-Path $root 'FirmwarePicker.inc'
$flowInclude = Join-Path $root 'FirmwareFlow.inc'
$previewInclude = Join-Path $root 'DiskPreview.inc'
$diskInclude = Join-Path $root 'DiskBackend.inc'
try {
    $source = [IO.File]::ReadAllText((Join-Path $project 'src\CP400Menu.cpp'))
    $start = $source.IndexOf('static char* ChooseSdFile(', [StringComparison]::Ordinal)
    $end = $source.IndexOf('void DisplayDiskContent(void)', [StringComparison]::Ordinal)
    if ($start -lt 0 -or $end -le $start) { throw 'Cannot locate production firmware picker.' }
    [IO.File]::WriteAllText($include, $source.Substring($start, $end - $start))
    $start = $source.IndexOf('void PopulateDiskContent(', [StringComparison]::Ordinal)
    $end = $source.IndexOf('void PrintFileName(', $start, [StringComparison]::Ordinal)
    if ($start -lt 0 -or $end -le $start) { throw 'Cannot locate production disk preview.' }
    [IO.File]::WriteAllText($previewInclude, $source.Substring($start, $end - $start))
    $start = $source.IndexOf('static void ShowFirmwareUpdateError(', [StringComparison]::Ordinal)
    $end = $source.IndexOf('void WaitForMenuKeyRelease(void)' + "`r", $start, [StringComparison]::Ordinal)
    if ($end -lt 0) { $end = $source.IndexOf('void WaitForMenuKeyRelease(void)' + "`n", $start, [StringComparison]::Ordinal) }
    if ($start -lt 0 -or $end -le $start) { throw 'Cannot locate production firmware confirmation flow.' }
    [IO.File]::WriteAllText($flowInclude, $source.Substring($start, $end - $start))
    $source = [IO.File]::ReadAllText((Join-Path $project 'src\CP400Emulator.cpp'))
    $start = $source.IndexOf('bool ReadCP400DiskImage(', [StringComparison]::Ordinal)
    $end = $source.IndexOf('bool EjectCP400Disk(', $start, [StringComparison]::Ordinal)
    if ($start -lt 0 -or $end -le $start) { throw 'Cannot locate production disk load/configuration.' }
    $backend = $source.Substring($start, $end - $start)
    $start = $source.IndexOf('bool WriteCP400DiskImage(', [StringComparison]::Ordinal)
    $end = $source.IndexOf('cpu_t cpu;', $start, [StringComparison]::Ordinal)
    if ($start -lt 0 -or $end -le $start) { throw 'Cannot locate production disk write.' }
    [IO.File]::WriteAllText($diskInclude, $backend + $source.Substring($start, $end - $start))
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$root" "-I$(Join-Path $project 'tests\browser_stubs')" `
        "-I$(Join-Path $project 'src')" (Join-Path $project 'src\Cp400SdBrowser.cpp') `
        (Join-Path $project 'src\Cp400Flh.cpp') (Join-Path $project 'tests\FirmwareBrowserTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'CP400 firmware browser test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'CP400 firmware browser regression failed.' }
}
finally {
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
    if (Test-Path -LiteralPath $include) { Remove-Item -LiteralPath $include }
    if (Test-Path -LiteralPath $flowInclude) { Remove-Item -LiteralPath $flowInclude }
    if (Test-Path -LiteralPath $previewInclude) { Remove-Item -LiteralPath $previewInclude }
    if (Test-Path -LiteralPath $diskInclude) { Remove-Item -LiteralPath $diskInclude }
    Remove-Item -LiteralPath $root
}
