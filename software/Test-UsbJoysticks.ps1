param([string[]]$Projects = @(
    'esp32_msx_emulator', 'esp32_msxturbor_emulator', 'esp32_apple2_emulator',
    'esp32_c64_emulator', 'esp32_sms_emulator', 'esp32_tk95_emulator'
))
$ErrorActionPreference = 'Stop'
$compiler = (Get-Command gcc -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$root = Join-Path ([System.IO.Path]::GetTempPath()) ('clone-usb-test-' + [Guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($root) | Out-Null
function Section([string]$Text, [string]$Start, [string]$End) {
    $a = $Text.IndexOf($Start, [StringComparison]::Ordinal)
    if ($a -lt 0) { throw "Cannot locate USB production section: $Start" }
    $b = $Text.IndexOf($End, $a + $Start.Length, [StringComparison]::Ordinal)
    if ($a -lt 0 -or $b -le $a) { throw "Cannot locate USB production section: $Start" }
    return $Text.Substring($a, $b - $a)
}
try {
    foreach ($project in $Projects) {
        $usb = Join-Path $PSScriptRoot ($project + '\lib\MsxSoftUsb\upstream')
        $source = [IO.File]::ReadAllText((Join-Path $usb 'usb_host.c')).Replace("`r", '')
        $header = [IO.File]::ReadAllText((Join-Path $usb 'usb_host.h')).Replace("`r", '')
        $definitions = @(
            (Section $source '#define T_START' '#define USB_LS_K'),
            (Section $header '// Device Descriptor' '#endif'),
            (Section $source "typedef struct`n{`n  uint8_t cmd;" '#ifdef WR_SIMULTA')
        )
        [IO.File]::WriteAllText((Join-Path $root 'UsbTypes.inc'), ($definitions -join "`n"))
        $logic = @(
            (Section $source 'void Request( ' 'void (*usbMess)'),
            (Section $source 'void fsm_Mashine()' 'void IRAM_ATTR setPins'),
            $source.Substring($source.IndexOf('void printState()')).Replace('#pragma GCC diagnostic pop', '')
        )
        [IO.File]::WriteAllText((Join-Path $root 'UsbLogic.inc'), ($logic -join "`n"))
        $output = Join-Path $root 'usb.exe'
        & $compiler -std=c11 -Wall -Wextra -Werror "-I$root" `
            (Join-Path $PSScriptRoot 'tests\UsbHostEnumerationTests.c') -o $output
        if ($LASTEXITCODE -ne 0) { throw "USB state-machine test compilation failed: $project" }
        Write-Host "Testing production USB enumeration and reports: $project"
        & $output
        if ($LASTEXITCODE -ne 0) { throw "USB state-machine regression failed: $project" }
    }
}
finally {
    $env:PATH = $oldPath
    foreach ($name in @('UsbTypes.inc', 'UsbLogic.inc', 'usb.exe')) {
        $path = Join-Path $root $name
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    Remove-Item -LiteralPath $root
}
