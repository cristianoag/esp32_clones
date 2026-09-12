param([string]$TinyMagicRom = '')
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$core = Join-Path $project 'lib\fmsx'
$root = Join-Path ([System.IO.Path]::GetTempPath()) ("msx-mapper-test-" + [Guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($root) | Out-Null
$files = @('MapperDatabase.o', 'SHA1.o', 'mapper.exe') | ForEach-Object { Join-Path $root $_ }
try {
    $includes = @("-I$core\EMULib", "-I$core\fMSX", "-I$core\Z80", "-I$core\MapperDatabase")
    foreach ($name in @('MapperDatabase', 'SHA1')) {
        & gcc -std=c99 -Wall -Wextra -Werror -Wno-error=old-style-declaration @includes `
            -c "$core\EMULib\$name.c" -o "$root\$name.o"
        if ($LASTEXITCODE -ne 0) { throw "Mapper test compilation failed: $name" }
    }
    & g++ -std=c++11 -Wall -Wextra -Werror @includes "$project\tests\MapperDatabaseTests.cpp" `
        "$root\MapperDatabase.o" "$root\SHA1.o" -o "$root\mapper.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Mapper test linking failed.' }
    if ($TinyMagicRom) { & "$root\mapper.exe" (Resolve-Path -LiteralPath $TinyMagicRom).Path }
    else { & "$root\mapper.exe" }
    if ($LASTEXITCODE -ne 0) { throw 'Mapper database regression failed.' }
}
finally {
    foreach ($file in $files) { if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file } }
    Remove-Item -LiteralPath $root
}
