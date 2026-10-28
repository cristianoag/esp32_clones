param(
    [string]$Firmware,
    [string]$Kanji,
    [string]$Cartridge,
    [string]$Game,
    [switch]$Rebuild
)
$ErrorActionPreference = 'Stop'
if (($Cartridge -or $Game) -and (!$Firmware -or !$Kanji)) {
    throw '-Cartridge/-Game require both -Firmware and -Kanji.'
}
if ($Cartridge -and $Game) { throw 'Choose either -Cartridge or -Game.' }
$root = Split-Path $PSScriptRoot -Parent
$core = Join-Path $root 'lib\bluemsx'
$build = Join-Path $root 'tests\build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $build 'original-state'),(Join-Path $build 'synthetic-state') -Force | Out-Null
$includes = @("-I$(Join-Path $core 'include')", "-I$(Join-Path $core 'port')")
Get-ChildItem -LiteralPath (Join-Path $core 'Src') -Directory | ForEach-Object {
    $includes += "-I$($_.FullName)"
}
$flags = @('-O2', '-g', '-ffunction-sections', '-fdata-sections',
           '-DZ80_CUSTOM_CONFIGURATION', '-DVIDEO_COLOR_TYPE_RGB565',
           '-include', (Join-Path $core 'include\A1stAllocator.h'))
$objects = @()
$sources = @(Get-ChildItem -LiteralPath (Join-Path $core 'Src'),(Join-Path $core 'port') -Recurse -File |
    Where-Object { $_.Extension -eq '.c' -or $_.Extension -eq '.cpp' })
$headers = Get-ChildItem -LiteralPath $core -Filter '*.h' -Recurse -File |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
foreach ($source in $sources) {
    $object = Join-Path $build ($source.Name + '.o')
    $objects += $object
    $sourceFlags = @()
    $hooksChanged = $false
    if ($source.Name -eq 'A1stCore.c') {
        $hooks = Join-Path $root 'tests\RomIoHooks.h'
        $sourceFlags = @('-include', $hooks)
        $hooksChanged = !(Test-Path -LiteralPath $object) -or
            (Get-Item -LiteralPath $hooks).LastWriteTime -gt (Get-Item -LiteralPath $object).LastWriteTime
    }
    if ($Rebuild -or !(Test-Path -LiteralPath $object) -or
        $hooksChanged -or
        $source.LastWriteTime -gt (Get-Item -LiteralPath $object).LastWriteTime -or
        $headers.LastWriteTime -gt (Get-Item -LiteralPath $object).LastWriteTime) {
        $compiler = 'gcc'
        $compatibility = @('-Wno-incompatible-pointer-types')
        if ($source.Extension -eq '.cpp') { $compiler = 'g++'; $compatibility = @() }
        & $compiler @flags @sourceFlags @compatibility @includes -c $source.FullName -o $object
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $($source.Name)" }
    }
}
$test = Join-Path $root 'tests\CoreTests.cpp'
$executable = Join-Path $build 'a1st-tests.exe'
& g++ '-O2' '-g' '-Wl,--gc-sections,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free' `
    @includes $test (Join-Path $root 'tests\MediaTests.cpp') `
    (Join-Path $root 'tests\AudioCoreTests.cpp') `
    (Join-Path $root 'tests\GameAudioTests.cpp') `
    (Join-Path $root 'tests\MemoryHooks.cpp') @objects '-o' $executable
if ($LASTEXITCODE -ne 0) { throw 'Host core link failed.' }
Push-Location $build
try {
    if ($Game) { & $executable $Firmware $Kanji '--game' $Game }
    elseif ($Firmware -and $Kanji -and $Cartridge) { & $executable $Firmware $Kanji $Cartridge }
    elseif ($Firmware -and $Kanji) { & $executable $Firmware $Kanji }
    else { & $executable }
    if ($LASTEXITCODE -ne 0) { throw "Core tests failed ($LASTEXITCODE)." }
} finally { Pop-Location }
