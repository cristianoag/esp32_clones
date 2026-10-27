$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$root = Join-Path ([IO.Path]::GetTempPath()) ('cp400-startup-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
try {
    foreach ($section in @(
        @('lib\SoftUSB\ESP32-USBSoftHost.hpp', '#ifndef BLINK_GPIO', '// include the modified', 'BlinkConfig.inc'),
        @('src\CP400UsbKeyboard.cpp', 'static void ClearKeyboardState(void)', '/*', 'ClearKeyboard.inc'),
        @('src\CP400Emulator.cpp', "void Setup_USB(void)`n{", '#define K_ESC', 'SetupUsb.inc')
    )) {
        $source = [IO.File]::ReadAllText((Join-Path $project $section[0])).Replace("`r", '')
        $start = $source.IndexOf($section[1], [StringComparison]::Ordinal)
        if ($start -lt 0) { throw "Cannot locate startup section: $($section[1])" }
        $end = $source.IndexOf($section[2], $start, [StringComparison]::Ordinal)
        if ($end -le $start) { throw "Cannot locate startup section end: $($section[2])" }
        [IO.File]::WriteAllText((Join-Path $root $section[3]), $source.Substring($start, $end - $start))
    }
    $video = [IO.File]::ReadAllText((Join-Path $project 'lib\ESP32-S3-VGA-main\src\VGA.cpp'))
    if ($video -match 'attachPinToSignal\s*\(\s*48\s*,') {
        throw 'VGA still routes a peripheral clock onto the RGB LED GPIO48.'
    }
    $output = Join-Path $root 'startup.exe'
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$root" `
        (Join-Path $project 'tests\StartupTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'CP400 startup test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'CP400 startup regression failed.' }
    Write-Host 'PASS: GPIO0 unclaimed, GPIO48 not clocked, keyboard starts before CPU timer, hotplug releases F12.'
}
finally {
    $env:PATH = $oldPath
    foreach ($name in @('BlinkConfig.inc', 'ClearKeyboard.inc', 'SetupUsb.inc', 'startup.exe')) {
        $path = Join-Path $root $name
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    Remove-Item -LiteralPath $root
}
