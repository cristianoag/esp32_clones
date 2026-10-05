$ErrorActionPreference = 'Stop'
$projects = @(
    @{ Folder = 'esp32_cp400_emulator'; Prefix = 'ESP32_CP400' },
    @{ Folder = 'esp32_msx_emulator'; Prefix = 'ESP32_MSX' },
    @{ Folder = 'esp32_tk95_emulator'; Prefix = 'ESP32_TK95' },
    @{ Folder = 'esp32_apple2_emulator'; Prefix = 'ESP32_APPLE2' }
)
$expectedLayout = @(
    'nvs,data,nvs,0x9000,0x5000,',
    'otadata,data,ota,0xe000,0x2000,',
    'app0,app,ota_0,0x10000,0x400000,',
    'app1,app,ota_1,0x410000,0x400000,'
) -join "`n"
$packages = foreach ($entry in $projects) {
    $project = Join-Path $PSScriptRoot $entry.Folder
    $layout = (Get-Content -LiteralPath (Join-Path $project 'partitions.csv') |
        Where-Object { $_.Trim() -and -not $_.Trim().StartsWith('#') } |
        ForEach-Object { $_ -replace '\s', '' }) -join "`n"
    if ($layout -ne $expectedLayout) { throw "Incompatible OTA layout: $project" }
    $config = Get-Content -Raw -LiteralPath (Join-Path $project 'platformio.ini')
    if ($config -notmatch '(?m)^board_build\.partitions\s*=\s*partitions\.csv\s*$') {
        throw "Build does not select the shared OTA layout: $project"
    }
    $makefile = Get-Content -Raw -LiteralPath (Join-Path $project 'Makefile')
    if ($makefile -notmatch '(?m)^FW_VERSION\s*[:?]?=\s*(\d+\.\d+)\s*$') {
        throw "Cannot find firmware version: $project"
    }
    $package = Join-Path $project ('dist\' + $entry.Prefix + '-' + $Matches[1] + '.FLH')
    if (-not (Test-Path -LiteralPath $package -PathType Leaf)) {
        throw "Build all four projects with make firmware first. Missing: $package"
    }
    $package
}
foreach ($entry in $projects) {
    Write-Host "Testing $($entry.Prefix) updater against all four destination packages"
    & (Join-Path $PSScriptRoot ($entry.Folder + '\tools\Test-FirmwareUpdate.ps1')) -PackagePaths $packages
}
Write-Host 'Shared OTA layout and all 16 firmware installation combinations passed (mock OTA).'
