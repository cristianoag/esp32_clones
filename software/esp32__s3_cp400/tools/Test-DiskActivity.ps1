$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$root = Join-Path ([IO.Path]::GetTempPath()) ('cp400-activity-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$names = @('DiskState.inc', 'DiskBytes.inc', 'DriveNumber.inc', 'DiskCommands.inc',
           'DiskRead.inc', 'DiskWrite.inc', 'DiskEject.inc', 'ActivityVideo.inc',
           'VideoModes.inc', 'VideoCore.inc', 'activity.exe')
try {
    $source = [IO.File]::ReadAllText((Join-Path $project 'src\CP400Emulator.cpp')).Replace("`r", '')
    foreach ($section in @(
        @('struct DiskAccessStruct', 'DriveStruct Disk_Drive;', 'DiskState.inc'),
        @('uint8_t ReadDiskByte(', 'uint8_t MountFileSystem(', 'DiskBytes.inc'),
        @('uint8_t GetDriveNumber(', 'void InitDisks(', 'DriveNumber.inc'),
        @('#define MACRO_TRACK_R', 'void ManagePeripherals_Read(', 'DiskCommands.inc'),
        @('bool EjectCP400Disk(', 'bool WriteCP400DiskImage(', 'DiskEject.inc'),
        @('static void DrawDiskActivity(', '#ifdef DEBUG_ALL', 'ActivityVideo.inc'),
        @('#define CP400_GRAPH_MODE_32X16_8X12', 'static void DrawDiskActivity(', 'VideoModes.inc'),
        @('void IRAM_ATTR VideoCore(', 'void RenderCP400GraphMode_64X64X4(', 'VideoCore.inc')
    )) {
        $start = $source.IndexOf($section[0], [StringComparison]::Ordinal)
        if ($start -lt 0) { throw "Cannot locate disk-activity section: $($section[0])" }
        $end = $source.IndexOf($section[1], $start, [StringComparison]::Ordinal)
        if ($end -le $start) { throw "Cannot locate disk-activity section end: $($section[1])" }
        [IO.File]::WriteAllText((Join-Path $root $section[2]), $source.Substring($start, $end - $start))
    }
    foreach ($section in @(
        @('void ManagePeripherals_Read(', 'case M_FF48:', 'default:', 'DiskRead.inc'),
        @('void ManagePeripherals_Write(', 'case M_FF40:', '//------------All the next logic is for videomodes', 'DiskWrite.inc')
    )) {
        $function = $source.IndexOf($section[0], [StringComparison]::Ordinal)
        if ($function -lt 0) { throw "Cannot locate disk controller: $($section[0])" }
        $start = $source.IndexOf($section[1], $function, [StringComparison]::Ordinal)
        if ($start -lt 0) { throw "Cannot locate disk register: $($section[1])" }
        $end = $source.IndexOf($section[2], $start, [StringComparison]::Ordinal)
        if ($end -le $start) { throw "Cannot locate disk register end: $($section[2])" }
        [IO.File]::WriteAllText((Join-Path $root $section[3]), $source.Substring($start, $end - $start))
    }
    $output = Join-Path $root 'activity.exe'
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$root" `
        "-I$(Join-Path $project 'src')" "-I$(Join-Path $project 'lib\ESP32-S3-VGA-main\src')" `
        (Join-Path $project 'lib\ESP32-S3-VGA-main\src\Mode.cpp') `
        (Join-Path $project 'tests\DiskActivityTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'CP400 disk-activity test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'CP400 disk-activity regression failed.' }
}
finally {
    $env:PATH = $oldPath
    foreach ($name in $names) {
        $path = Join-Path $root $name
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    Remove-Item -LiteralPath $root
}
