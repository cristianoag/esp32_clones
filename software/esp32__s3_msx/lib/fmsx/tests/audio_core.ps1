param([string[]]$FmBios = @())
$ErrorActionPreference = 'Stop'
$core = (Resolve-Path "$PSScriptRoot\..").Path
$build = "$PSScriptRoot\.build\audio"
$previous = Get-Location
$previousTmp = $env:TMP
$previousTemp = $env:TEMP
New-Item -ItemType Directory -Force $build | Out-Null
$flags = @('-O0', '-DLSB_FIRST', '-DBPP8', '-DBPS16', '-DNARROW', '-DFMSX',
  "-I$core", "-I$core\fMSX", "-I$core\EMULib", "-I$core\Z80")
$sources = @("$core\fMSX\MSX.c", "$core\fMSX\V9938.c", "$core\fMSX\Patch.c",
  "$core\fMSX\I8251.c", "$core\Z80\Z80.c")
$sources += @(Get-ChildItem "$core\EMULib\*.c" | ForEach-Object FullName)
try {
  Set-Location $build
  $env:TMP = $env:TEMP = $build
  & gcc @flags -std=gnu99 -c @sources
  if ($LASTEXITCODE -ne 0) { throw 'Audio core compilation failed.' }
  & g++ @flags -std=gnu++11 "$PSScriptRoot\audio_core.cpp" "$core\..\..\src\MsxAudioProfiles.cpp" `
    @(Get-ChildItem '*.o' | ForEach-Object FullName) -o audio_core.exe
  if ($LASTEXITCODE -ne 0) { throw 'Audio test linking failed.' }
  & '.\audio_core.exe' @FmBios
  if ($LASTEXITCODE -ne 0) { throw 'Audio regression failed.' }
} finally {
  foreach ($name in @('MSX.ROM', 'cart-a.rom', 'cart-b.rom', 'fm16.rom', 'fm64.rom',
                      'fm64.sav', 'invalid.rom', 'shared.rom', 'private-fm.rom', 'private-fm.sav',
                      'MSX2.ROM', 'MSX2EXT.ROM', 'DISK.ROM', 'MSXDOS2.ROM')) {
    $file = Join-Path $build $name
    if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file }
  }
  $preferred = Join-Path $build 'preferred'
  if (Test-Path -LiteralPath $preferred) { Remove-Item -LiteralPath $preferred -Recurse }
  Set-Location $previous
  $env:TMP = $previousTmp
  $env:TEMP = $previousTemp
}
