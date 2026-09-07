$ErrorActionPreference = 'Stop'
$importer = Join-Path $PSScriptRoot 'Import-Roms.ps1'
$root = Join-Path ([System.IO.Path]::GetTempPath()) ("msx-rom-test-" + [Guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($root) | Out-Null
$checks = 0

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "FAILED: $Message" }
    $script:checks++
}

function Assert-Rejected([scriptblock]$Action, [string]$Message) {
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    Assert-True $rejected $Message
}

try {
    $omega = [byte[]]::new(524288)
    for ($i = 0; $i -lt $omega.Length; $i++) { $omega[$i] = [byte](($i -shr 16) + 16) }
    $omegaPath = Join-Path $root 'omega.bin'
    $biosPath = Join-Path $root 'bios.rom'
    $subPath = Join-Path $root 'sub.rom'
    [System.IO.File]::WriteAllBytes($omegaPath, $omega)
    [System.IO.File]::WriteAllBytes($biosPath, [byte[]]::new(32768))
    [System.IO.File]::WriteAllBytes($subPath, [byte[]]::new(16384))
    $destination = Join-Path $root 'card'
    & $importer -Destination $destination -OmegaRom $omegaPath -ExpertRom $biosPath -HotbitRom $biosPath | Out-Null
    $biosRoot = Join-Path $destination 'msx\bios'
    $main = [System.IO.File]::ReadAllBytes((Join-Path $biosRoot 'omega\MSX2P.ROM'))
    $sub = [System.IO.File]::ReadAllBytes((Join-Path $biosRoot 'omega\MSX2PEXT.ROM'))
    Assert-True ($main.Length -eq 32768 -and ($main | Where-Object { $_ -ne 16 }).Count -eq 0) 'First Omega bank main ROM content'
    Assert-True ($sub.Length -eq 16384 -and ($sub | Where-Object { $_ -ne 17 }).Count -eq 0) 'Omega sub-ROM at +0x10000, not logo at +0x8000'
    Assert-True ((Get-Item -LiteralPath (Join-Path $biosRoot 'expert\MSX.ROM')).Length -eq 32768) 'Expert ROM import'
    Assert-True ((Get-Item -LiteralPath (Join-Path $biosRoot 'hotbit\MSX.ROM')).Length -eq 32768) 'Hotbit ROM import'
    Assert-True ((Get-Content -Raw -LiteralPath (Join-Path $biosRoot 'omega\profile.ini')) -eq "name=Omega MSX2+ NTSC (bank 0)`nmodel=MSX2+`nram=512`n") 'Firmware-compatible manifest'
    $hash = (Get-FileHash -LiteralPath (Join-Path $biosRoot 'omega\MSX2P.ROM')).Hash
    Assert-True ((Get-Content -Raw -LiteralPath (Join-Path $biosRoot 'omega\SHA256SUMS.txt')).Contains("$hash  MSX2P.ROM")) 'SHA256 manifest'
    Assert-Rejected { & $importer -Destination $destination -OmegaRom $omegaPath -OmegaBank 1 } 'No implicit overwrite'
    & $importer -Destination $destination -OmegaRom $omegaPath -OmegaBank 1 -Force | Out-Null
    $main = [System.IO.File]::ReadAllBytes((Join-Path $biosRoot 'omega\MSX2P.ROM'))
    $sub = [System.IO.File]::ReadAllBytes((Join-Path $biosRoot 'omega\MSX2PEXT.ROM'))
    Assert-True (($main | Where-Object { $_ -ne 20 }).Count -eq 0) 'Second Omega bank main ROM content'
    Assert-True (($sub | Where-Object { $_ -ne 21 }).Count -eq 0) 'Second Omega bank extension content'
    $singleBank = Join-Path $root 'single.bin'
    [System.IO.File]::WriteAllBytes($singleBank, [byte[]]::new(262144))
    Assert-Rejected { & $importer -Destination $destination -OmegaRom $singleBank -OmegaBank 1 -Force } 'Reject bank 1 for a 256 KiB image'
    Assert-Rejected { & $importer -Destination $destination -ExpertRom $subPath -Force } 'Reject short MSX1 BIOS'
    Assert-Rejected { & $importer -Destination $destination -BiosRom $biosPath -Model MSX2 -ProfileId custom -Name Custom } 'Require MSX2 extension'
    Assert-Rejected { & $importer -Destination $destination -BiosRom $biosPath -Model MSX1 -ProfileId custom -Name Custom -ExtensionRom $subPath } 'Reject extension for MSX1'
    Assert-Rejected { & $importer -Destination $destination -BiosRom $biosPath -ProfileId '..\outside' -Name Custom } 'Reject path traversal'
    Assert-Rejected { & $importer -Destination $destination -BiosRom $biosPath -ProfileId custom -Name "bad`nmodel=MSX2" } 'Reject manifest injection'
    Assert-Rejected { & $importer -Destination $destination -BiosRom $biosPath -ProfileId omega -Name Custom } 'Protect reserved profile IDs'
    Assert-Rejected { & $importer -Destination $destination } 'Require a ROM input'
    & $importer -Destination $destination -BiosRom $biosPath -ExtensionRom $subPath -Model MSX2 -ProfileId custom2 -Name 'Custom MSX2' | Out-Null
    Assert-True (Test-Path -LiteralPath (Join-Path $biosRoot 'custom2\MSX2EXT.ROM')) 'Custom MSX2 profile'
    & $importer -Destination $destination -BiosRom $biosPath -ExtensionRom $subPath -Model MSX2+ -ProfileId custom2p -Name 'Custom MSX2+' | Out-Null
    Assert-True (Test-Path -LiteralPath (Join-Path $biosRoot 'custom2p\MSX2PEXT.ROM')) 'Custom MSX2+ profile'
    & $importer -Destination $destination -BiosRom $biosPath -ProfileId custom1 -Name 'Custom MSX1' | Out-Null
    Assert-True (Test-Path -LiteralPath (Join-Path $biosRoot 'custom1\MSX.ROM')) 'Custom MSX1 profile'
    $invalidBatch = Join-Path $root 'invalid-batch'
    Assert-Rejected { & $importer -Destination $invalidBatch -OmegaRom $omegaPath -ExpertRom $subPath } 'Validate entire batch before writing'
    Assert-True (-not (Test-Path -LiteralPath $invalidBatch)) 'Rejected batch left no output'
    Write-Output "All $checks ROM import checks passed."
}
finally {
    # Only remove the uniquely named fixtures created by this test, leaf first.
    Get-ChildItem -LiteralPath $root -File -Recurse | ForEach-Object { Remove-Item -LiteralPath $_.FullName }
    Get-ChildItem -LiteralPath $root -Directory -Recurse | Sort-Object { $_.FullName.Length } -Descending |
        ForEach-Object { Remove-Item -LiteralPath $_.FullName }
    Remove-Item -LiteralPath $root
}
