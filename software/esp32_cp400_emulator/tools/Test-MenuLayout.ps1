$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$root = Join-Path ([IO.Path]::GetTempPath()) ('cp400-menu-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
try {
    foreach ($section in @(
        @('src\CP400Menu.cpp', 'constexpr uint8_t MENU_TEXT_COLUMNS', '#ifdef PSRAM_EMU', 'MenuLayout.inc'),
        @('src\CP400Menu.cpp', 'void DrawMenuLine(', 'void PopulateDiskContent(', 'MenuFrame.inc'),
        @('src\CP400Menu.cpp', 'void MENU_DisplayGIMEchar(', 'void EMULATOR_Menu(', 'MenuText.inc'),
        @('src\CP400Menu.cpp', 'void BackDisplay(', 'enum MainMenuItem', 'MenuBackup.inc'),
        @('src\CP400Menu.cpp', 'enum MainMenuItem', 'void ChangeMainMenuValue(', 'MenuRows.inc'),
        @('src\CP400Menu.cpp', 'void DrawMainMenuOptions(', 'static char* ChooseSdFile(', 'MenuOptions.inc'),
        @('src\CP400Menu.cpp', 'void DisplayDiskContent(', '#define FRAME_COL_1', 'MenuPreview.inc'),
        @('src\CP400Emulator.cpp', "void line(int x0", '//---------------------------------------USB STACK', 'MenuLine.inc')
    )) {
        $source = [IO.File]::ReadAllText((Join-Path $project $section[0])).Replace("`r", '')
        $start = $source.IndexOf($section[1], [StringComparison]::Ordinal)
        if ($start -lt 0) { throw "Cannot locate menu section: $($section[1])" }
        $end = $source.IndexOf($section[2], $start, [StringComparison]::Ordinal)
        if ($end -le $start) { throw "Cannot locate menu section end: $($section[2])" }
        $text = $source.Substring($start, $end - $start)
        if ($section[3] -eq 'MenuPreview.inc') {
            # DrawFrames is the last function in the production menu source.
            $text = $source.Substring($start)
        }
        [IO.File]::WriteAllText((Join-Path $root $section[3]), $text)
    }
    $output = Join-Path $root 'menu.exe'
    & $compiler -std=c++11 -Wall -Wextra -Werror "-I$root" `
        "-I$(Join-Path $project 'src')" "-I$(Join-Path $project 'lib\ESP32-S3-VGA-main\src')" `
        (Join-Path $project 'lib\ESP32-S3-VGA-main\src\Mode.cpp') `
        (Join-Path $project 'tests\MenuLayoutTests.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'CP400 menu layout test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'CP400 menu layout regression failed.' }
}
finally {
    $env:PATH = $oldPath
    foreach ($name in @('MenuLayout.inc', 'MenuFrame.inc', 'MenuText.inc', 'MenuBackup.inc',
                       'MenuRows.inc', 'MenuOptions.inc', 'MenuPreview.inc', 'MenuLine.inc', 'menu.exe')) {
        $path = Join-Path $root $name
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    Remove-Item -LiteralPath $root
}
