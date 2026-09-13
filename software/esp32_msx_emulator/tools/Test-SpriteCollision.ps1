$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$output = Join-Path ([IO.Path]::GetTempPath()) ("msx-sprite-" + [Guid]::NewGuid().ToString('N') + '.exe')
try {
    & g++ -O2 -std=c++11 -Wall -Wextra -Werror (Join-Path $project 'lib\fmsx\tests\sprite_collision.cpp') -o $output
    if ($LASTEXITCODE -ne 0) { throw 'Sprite collision test build failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Sprite collision regression failed.' }
}
finally {
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
}
