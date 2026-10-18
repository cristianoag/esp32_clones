$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction Stop).Source
$cCompiler = Join-Path (Split-Path -Parent $compiler) 'gcc.exe'
$output = Join-Path ([IO.Path]::GetTempPath()) ('macplus-tests-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($output) | Out-Null
$oldPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
    $includes = @("-I$(Join-Path $project 'src')", "-I$(Join-Path $project 'src\tme')",
                  "-I$(Join-Path $project 'src\tme\musashi')")
    $sources = @('emu.c','via.c','scc.c','iwm.c','mouse.c','rtc.c','ncr.c',
                 'musashi\m68kcpu.c','musashi\m68kopac.c','musashi\m68kopdm.c',
                 'musashi\m68kopnz.c','musashi\m68kops_pre.c')
    $objects = foreach ($source in $sources) {
        $object = Join-Path $output (($source -replace '\\','-') + '.o')
        & $cCompiler -std=gnu11 -O0 @includes -c (Join-Path $project ('src\tme\' + $source)) -o $object
        if ($LASTEXITCODE -ne 0) { throw "Core compilation failed: $source" }
        $object
    }
    $exe = Join-Path $output 'macplus-tests.exe'
    & $compiler -std=c++11 -O2 -Wall -Wextra -Werror @includes `
        (Join-Path $project 'tests\MacTests.cpp') (Join-Path $project 'src\MacDisk.cpp') `
        (Join-Path $project 'src\MacScsi.cpp') @objects -o $exe
    if ($LASTEXITCODE -ne 0) { throw 'Mac Plus test compilation failed.' }
    & $exe (Join-Path $output 'test-disk.img')
    if ($LASTEXITCODE -ne 0) { throw 'Mac Plus regression failed.' }
}
finally {
    $env:PATH = $oldPath
    Remove-Item -LiteralPath $output -Recurse -Force
}
