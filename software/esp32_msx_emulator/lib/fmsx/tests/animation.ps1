param(
  [string]$BiosDirectory = '',
  [string]$Sdk = "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif32\tools\sdk\esp32s3",
  [ValidateRange(1,36000)][int]$Frames = 900,
  [ValidateRange(1,100)][int]$DrawPercent = 100,
  [ValidateSet(4,8,16,32)][int]$RamPages = 32,
  [switch]$Capture,
  [switch]$Raster
)
$ErrorActionPreference = 'Stop'
$core = (Resolve-Path "$PSScriptRoot\..").Path
if ($BiosDirectory) { $biosSource = (Resolve-Path -LiteralPath $BiosDirectory).Path }
$build = "$PSScriptRoot\.build\animation"
$previous = Get-Location
New-Item -ItemType Directory -Force "$build\bios" | Out-Null
$flags = @('-O2', '-DLSB_FIRST', '-DBPP8', '-DBPS16', '-DNARROW', '-DFMSX',
  "-I$core", "-I$core\fMSX", "-I$core\EMULib", "-I$core\Z80")
if ($Raster) { $flags += '-DDEBUG' }
$sources = @("$core\fMSX\MSX.c", "$core\fMSX\V9938.c", "$core\fMSX\Patch.c",
  "$core\fMSX\I8251.c", "$core\Z80\Z80.c")
$sources += @(Get-ChildItem "$core\EMULib\*.c" | ForEach-Object FullName)
try {
  Set-Location $build
  & gcc @flags -std=gnu99 -c @sources
  if ($LASTEXITCODE -ne 0) { throw 'Animation core compilation failed.' }
  & g++ @flags -std=gnu++11 "-I$Sdk\include\heap\include" "-I$Sdk\qio_opi\include" `
    "-I$Sdk\include\esp_common\include" "$PSScriptRoot\vdp_animation.cpp" `
    @(Get-ChildItem '*.o' | ForEach-Object FullName) -o vdp_animation.exe
  if ($LASTEXITCODE -ne 0) { throw 'VDP animation test compilation failed.' }
  & '.\vdp_animation.exe'
  if ($LASTEXITCODE -ne 0) { throw 'VDP animation regression failed.' }
  if (!$BiosDirectory) { return }
  $romName = if (Test-Path -LiteralPath "$biosSource\OMEGA.ROM") { 'OMEGA.ROM' }
             elseif (Test-Path -LiteralPath "$biosSource\PANASONIC.ROM") { 'PANASONIC.ROM' }
             else { throw 'Animation capture requires OMEGA.ROM or PANASONIC.ROM in the BIOS directory.' }
  Copy-Item -LiteralPath "$biosSource\$romName" -Destination "$build\bios"
  & g++ @flags -std=gnu++11 "-I$Sdk\include\heap\include" "-I$Sdk\qio_opi\include" `
    "-I$Sdk\include\esp_common\include" "$core\..\..\src\MsxCore.cpp" `
    "$PSScriptRoot\animation.cpp" @(Get-ChildItem '*.o' | ForEach-Object FullName) `
    -o animation.exe
  if ($LASTEXITCODE -ne 0) { throw 'Animation host linking failed.' }
  Get-ChildItem -Path 'animation-*.ppm', 'raster.csv' -File -ErrorAction SilentlyContinue | Remove-Item
  if ($Capture) { $env:FMSX_CAPTURE = '1' }
  if ($Raster) { $env:FMSX_RASTER = '1' }
  $posix = "$build\bios".Replace('\', '/')
  if ($posix[1] -eq ':') { $posix = $posix.Substring(2) }
  & '.\animation.exe' $posix $Frames $DrawPercent $RamPages
  if ($LASTEXITCODE -ne 0) { throw 'Animation verification failed; inspect animation.csv and captures.' }
} finally {
  Remove-Item Env:FMSX_CAPTURE, Env:FMSX_RASTER -ErrorAction SilentlyContinue
  Get-ChildItem "$build\bios" -File | Remove-Item
  Set-Location $previous
}
