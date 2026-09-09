param(
  [string]$Sdk = "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif32\tools\sdk\esp32s3",
  [string]$BiosDirectory = '',
  [string]$ProfilesRoot = '',
  [ValidateRange(0,2)][int]$Model = 0,
  [ValidateSet(4,8,16,32)][int]$RamPages = 4,
  [ValidateRange(1,36000)][int]$Frames = 900
)
$ErrorActionPreference = 'Stop'
if ($BiosDirectory -and $ProfilesRoot) { throw 'Use either -BiosDirectory or -ProfilesRoot, not both.' }
$profiles = @()
if ($BiosDirectory) {
  $profiles += @{ Directory = (Resolve-Path -LiteralPath $BiosDirectory).Path; Name = 'custom'; Model = $Model; Ram = $RamPages }
}
if ($ProfilesRoot) {
  foreach ($name in @('expert', 'hotbit', 'omega')) {
    $profileModel = if ($name -eq 'omega') { 2 } else { 0 }
    $ram = if ($name -eq 'omega') { 32 } else { 4 }
    $profiles += @{ Directory = (Resolve-Path -LiteralPath (Join-Path $ProfilesRoot $name)).Path; Name = $name; Model = $profileModel; Ram = $ram }
  }
}
$core = (Resolve-Path "$PSScriptRoot\..").Path
$build = "$PSScriptRoot\.build"
$previous = Get-Location
New-Item -ItemType Directory -Force $build | Out-Null
$flags = @('-O0', '-DLSB_FIRST', '-DBPP8', '-DBPS16', '-DNARROW', '-DFMSX',
  "-I$core", "-I$core\fMSX", "-I$core\EMULib", "-I$core\Z80")
$sources = @("$core\fMSX\MSX.c", "$core\fMSX\V9938.c", "$core\fMSX\Patch.c",
  "$core\fMSX\I8251.c", "$core\Z80\Z80.c")
$sources += @(Get-ChildItem "$core\EMULib\*.c" | ForEach-Object FullName)
try {
  Set-Location $build
  & g++ -O2 -std=gnu++11 "$PSScriptRoot\frame_pacer.cpp" -o frame_pacer.exe
  if ($LASTEXITCODE -ne 0) { throw 'Frame pacing test compilation failed.' }
  & '.\frame_pacer.exe'
  if ($LASTEXITCODE -ne 0) { throw 'Frame pacing regression failed.' }
  & gcc @flags -std=gnu99 -c @sources
  if ($LASTEXITCODE -ne 0) { throw 'Core C compilation failed.' }
  & g++ @flags -std=gnu++11 "-I$Sdk\include\heap\include" "-I$Sdk\qio_opi\include" `
    "-I$Sdk\include\esp_common\include" "$core\..\..\src\MsxCore.cpp" `
    "$PSScriptRoot\host_smoke.cpp" @(Get-ChildItem '*.o' | ForEach-Object FullName) `
    -o host_smoke.exe
  if ($LASTEXITCODE -ne 0) { throw 'Core host linking failed.' }
  & '.\host_smoke.exe'
  if ($LASTEXITCODE -ne 0) { throw 'Core host regression failed.' }
  foreach ($profile in $profiles) {
    # Prefer the single Omega bank; never write CMOS or test images into originals.
    $bios = "$build\bios"
    New-Item -ItemType Directory -Force $bios | Out-Null
    $names = switch ($profile.Model) {
      0 { @('MSX.ROM') }
      1 { @('MSX2.ROM', 'MSX2EXT.ROM') }
      2 { @('MSX2P.ROM', 'MSX2PEXT.ROM') }
    }
    $combined = $profile.Model -eq 2 -and
                (Test-Path -LiteralPath (Join-Path $profile.Directory 'OMEGA.ROM'))
    $logo = $combined -or ($profile.Model -eq 2 -and
            (Test-Path -LiteralPath (Join-Path $profile.Directory 'MSX2PLOGO.ROM')))
    if ($combined) { $names = @('OMEGA.ROM') }
    elseif ($logo) { $names += 'MSX2PLOGO.ROM' }
    try {
      foreach ($name in @('boot-logo.ppm', 'boot-early.ppm', "boot-logo-$($profile.Name).ppm")) {
        $capture = Join-Path $build $name
        if (Test-Path -LiteralPath $capture) { Remove-Item -LiteralPath $capture }
      }
      foreach ($name in $names) { Copy-Item -LiteralPath (Join-Path $profile.Directory $name) -Destination $bios }
      $posix = $bios.Replace('\', '/')
      if ($posix[1] -eq ':') { $posix = $posix.Substring(2) }
      Write-Output "Booting $($profile.Name)..."
      & '.\host_smoke.exe' $posix $profile.Model $profile.Ram $Frames
      if ($LASTEXITCODE -ne 0) { throw 'Real BIOS boot/logo verification failed; inspect the PPM captures and transcript.' }
      Copy-Item -LiteralPath "$build\boot.ppm" -Destination "$build\boot-$($profile.Name).ppm"
      Copy-Item -LiteralPath "$build\boot-early.ppm" -Destination "$build\boot-early-$($profile.Name).ppm"
      if ($logo -and (Test-Path -LiteralPath "$build\boot-logo.ppm")) {
        Copy-Item -LiteralPath "$build\boot-logo.ppm" -Destination "$build\boot-logo-$($profile.Name).ppm"
      }
    } finally {
      foreach ($name in @($names) + @('CMOS.ROM', 'MSX2PLOGO.ROM', 'OMEGA.ROM')) {
        $copy = Join-Path $bios $name
        if (Test-Path -LiteralPath $copy) { Remove-Item -LiteralPath $copy }
      }
    }
  }
} finally {
  foreach ($name in @('MSX.ROM', 'MSX2.ROM', 'MSX2EXT.ROM', 'MSX2P.ROM', 'MSX2PEXT.ROM', 'MSX2PLOGO.ROM', 'OMEGA.ROM',
                      'slot1.rom', 'slot2.rom', 'slot1.sav', 'slot2.sav', 'CARTS.CRC')) {
    $generated = Join-Path $build $name
    if (Test-Path -LiteralPath $generated) { Remove-Item -LiteralPath $generated }
  }
  Set-Location $previous
}
