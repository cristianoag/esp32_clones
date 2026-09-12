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
    for ($i = 0x08000; $i -lt 0x0C000; $i++) { $omega[$i] = 42; $omega[$i + 0x40000] = 43 }
    $omegaPath = Join-Path $root 'omega.bin'
    $biosPath = Join-Path $root 'bios.rom'
    $subPath = Join-Path $root 'sub.rom'
    [System.IO.File]::WriteAllBytes($omegaPath, $omega)
    [System.IO.File]::WriteAllBytes($biosPath, [byte[]]::new(32768))
    [System.IO.File]::WriteAllBytes($subPath, [byte[]]::new(16384))
    $destination = Join-Path $root 'card'
    & $importer -Destination $destination -OmegaRom $omegaPath -ExpertRom $biosPath -HotbitRom $biosPath | Out-Null
    $biosRoot = Join-Path $destination 'msx\bios'
    $bank = [System.IO.File]::ReadAllBytes((Join-Path $biosRoot 'omega\OMEGA.ROM'))
    Assert-True ($bank.Length -eq 262144) 'Omega is one complete 256 KiB bank'
    $same = $true
    for ($i = 0; $i -lt $bank.Length; $i++) { if ($bank[$i] -ne $omega[$i]) { $same = $false; break } }
    Assert-True $same 'First Omega bank preserved byte-for-byte'
    Assert-True ($bank[0] -eq 16 -and $bank[0x8000] -eq 42 -and $bank[0x10000] -eq 17) 'Main, logo and sub-ROM retained at their original offsets'
    Assert-True ((Get-ChildItem -LiteralPath (Join-Path $biosRoot 'omega') -Filter '*.ROM').Count -eq 1) 'Exactly one Omega ROM file'
    Assert-True ((Get-Item -LiteralPath (Join-Path $biosRoot 'expert\MSX.ROM')).Length -eq 32768) 'Expert ROM import'
    Assert-True ((Get-Item -LiteralPath (Join-Path $biosRoot 'hotbit\MSX.ROM')).Length -eq 32768) 'Hotbit ROM import'
    Assert-True ((Get-Content -Raw -LiteralPath (Join-Path $biosRoot 'omega\profile.ini')) -eq "name=Omega MSX2+ NTSC (bank 0)`nmodel=MSX2+`nram=512`n") 'Firmware-compatible manifest'
    $hash = (Get-FileHash -LiteralPath (Join-Path $biosRoot 'omega\OMEGA.ROM')).Hash
    Assert-True ((Get-Content -Raw -LiteralPath (Join-Path $biosRoot 'omega\SHA256SUMS.txt')).Contains("$hash  OMEGA.ROM")) 'Combined ROM checksum'
    Assert-Rejected { & $importer -Destination $destination -OmegaRom $omegaPath -OmegaBank 1 } 'No implicit overwrite'
    foreach ($legacy in @('MSX2P.ROM', 'MSX2PEXT.ROM', 'MSX2PLOGO.ROM', 'DISK.ROM')) {
        [System.IO.File]::WriteAllBytes((Join-Path $biosRoot "omega\$legacy"), [byte[]]::new(16384))
    }
    & $importer -Destination $destination -OmegaRom $omegaPath -OmegaBank 1 -Force | Out-Null
    $bank = [System.IO.File]::ReadAllBytes((Join-Path $biosRoot 'omega\OMEGA.ROM'))
    $same = $bank.Length -eq 262144
    for ($i = 0; $i -lt $bank.Length; $i++) { if ($bank[$i] -ne $omega[$i + 262144]) { $same = $false; break } }
    Assert-True $same 'Second Omega bank preserved byte-for-byte'
    Assert-True ($bank[0] -eq 20 -and $bank[0x8000] -eq 43 -and $bank[0x10000] -eq 21) 'Second bank logo and extension remain distinct'
    foreach ($legacy in @('MSX2P.ROM', 'MSX2PEXT.ROM', 'MSX2PLOGO.ROM')) {
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $biosRoot "omega\$legacy"))) "Removed obsolete imported $legacy"
    }
    Assert-True (Test-Path -LiteralPath (Join-Path $biosRoot 'omega\DISK.ROM')) 'Unrelated optional ROM retained'
    $singleBank = Join-Path $root 'single.bin'
    [System.IO.File]::WriteAllBytes($singleBank, [byte[]]::new(262144))
    Assert-Rejected { & $importer -Destination $destination -OmegaRom $singleBank -OmegaBank 1 -Force } 'Reject bank 1 for a 256 KiB image'
    $singleDestination = Join-Path $root 'single-card'
    & $importer -Destination $singleDestination -OmegaRom $singleBank | Out-Null
    Assert-True ((Get-FileHash -LiteralPath $singleBank).Hash -eq
        (Get-FileHash -LiteralPath (Join-Path $singleDestination 'msx\bios\omega\OMEGA.ROM')).Hash) 'Standalone 256 KiB image preserved unchanged'
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
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $biosRoot 'custom1\MSX2PLOGO.ROM'))) 'MSX1 has no implicit logo ROM'
    $invalidBatch = Join-Path $root 'invalid-batch'
    Assert-Rejected { & $importer -Destination $invalidBatch -OmegaRom $omegaPath -ExpertRom $subPath } 'Validate entire batch before writing'
    Assert-True (-not (Test-Path -LiteralPath $invalidBatch)) 'Rejected batch left no output'
    Assert-Rejected { & $importer -Destination $invalidBatch -ExpertRom $biosPath -DiskBios $biosPath } 'Reject wrong disk BIOS size before writing'
    Assert-True (-not (Test-Path -LiteralPath $invalidBatch)) 'Invalid disk BIOS left no profile output'
    $diskCard = Join-Path $root 'disk-card'
    & $importer -Destination $diskCard -OmegaRom $omegaPath -ExpertRom $biosPath -HotbitRom $biosPath -DiskBios $subPath | Out-Null
    $diskHash = (Get-FileHash -LiteralPath $subPath).Hash
    foreach ($id in @('omega', 'expert', 'hotbit')) {
        $folder = Join-Path $diskCard "msx\bios\$id"
        Assert-True ((Get-FileHash -LiteralPath (Join-Path $folder 'DISK.ROM')).Hash -eq $diskHash) "$id optional disk BIOS preserved byte-for-byte"
        Assert-True ((Get-Content -Raw -LiteralPath (Join-Path $folder 'SHA256SUMS.txt')).Contains("$diskHash  DISK.ROM")) "$id disk BIOS checksum"
    }
    $panasonic = Join-Path $root 'panasonic'
    [System.IO.Directory]::CreateDirectory($panasonic) | Out-Null
    foreach ($modelName in @('fs-a1wsx', 'fs-a1f', 'fs-a1fx')) {
        $biosSuffix = if ($modelName -eq 'fs-a1f') { 'basic-bios2.rom' } else { 'basic-bios2p.rom' }
        $subSuffix = if ($modelName -eq 'fs-a1f') { 'msx2sub.rom' } else { 'msx2psub.rom' }
        $fontSize = if ($modelName -eq 'fs-a1wsx') { 262144 } else { 131072 }
        foreach ($part in @(@($biosSuffix,32768,17), @($subSuffix,16384,34),
                            @('kanjibasic.rom',32768,51), @('kanjifont.rom',$fontSize,68))) {
            $data = [byte[]]::new($part[1])
            for ($i = 0; $i -lt $data.Length; ++$i) { $data[$i] = $part[2] }
            [System.IO.File]::WriteAllBytes((Join-Path $panasonic ($modelName + '_' + $part[0])), $data)
        }
    }
    $panCard = Join-Path $root 'panasonic-card'
    & $importer -Destination $panCard -PanasonicDirectory $panasonic -DiskBios $subPath | Out-Null
    foreach ($modelName in @('fs-a1wsx', 'fs-a1f', 'fs-a1fx')) {
        $profile = Join-Path $panCard "msx\bios\$modelName"
        Assert-True ((Get-FileHash -LiteralPath (Join-Path $profile 'DISK.ROM')).Hash -eq $diskHash) "$modelName optional disk BIOS"
        $image = [System.IO.File]::ReadAllBytes((Join-Path $profile 'PANASONIC.ROM'))
        $expected = if ($modelName -eq 'fs-a1wsx') { 344064 } else { 212992 }
        Assert-True ($image.Length -eq $expected) "$modelName combined size"
        foreach ($part in @(@(0,32768,17), @(0x8000,16384,34), @(0xC000,32768,51),
                            @(0x14000,($expected-0x14000),68))) {
            $equal = $true
            for ($i = $part[0]; $i -lt $part[0] + $part[1]; ++$i) {
                if ($image[$i] -ne $part[2]) { $equal = $false; break }
            }
            Assert-True $equal "$modelName region $($part[0]) is byte-identical"
        }
        Assert-True ((Get-ChildItem -LiteralPath $profile -Filter '*.ROM').Count -eq 2) "$modelName uses one system ROM plus the optional disk BIOS"
        $expectedModel = if ($modelName -eq 'fs-a1f') { 'MSX2' } else { 'MSX2+' }
        Assert-True ((Get-Content -Raw -LiteralPath (Join-Path $profile 'profile.ini')) -eq
            "name=Panasonic $($modelName.ToUpperInvariant())`nmodel=$expectedModel`nram=64`n") "$modelName hardware defaults"
        $hash = (Get-FileHash -LiteralPath (Join-Path $profile 'PANASONIC.ROM')).Hash
        Assert-True ((Get-Content -Raw -LiteralPath (Join-Path $profile 'SHA256SUMS.txt')).Contains("$hash  PANASONIC.ROM")) "$modelName checksum"
    }
    $oneCard = Join-Path $root 'one-panasonic-card'
    & $importer -Destination $oneCard -PanasonicDirectory $panasonic -PanasonicModels 'FS-A1FX' | Out-Null
    Assert-True ((Get-ChildItem -LiteralPath (Join-Path $oneCard 'msx\bios') -Directory).Count -eq 1) 'Import selected Panasonic only'
    Assert-Rejected { & $importer -Destination $panCard -PanasonicDirectory $panasonic } 'Panasonic overwrite requires Force'
    Assert-Rejected { & $importer -Destination $destination -BiosRom $biosPath -ProfileId 'fs-a1f' -Name Custom } 'Panasonic profile IDs reserved'
    [System.IO.File]::WriteAllBytes((Join-Path $panasonic 'fs-a1fx_kanjifont.rom'), [byte[]]::new(32768))
    $badPanCard = Join-Path $root 'bad-panasonic-card'
    Assert-Rejected { & $importer -Destination $badPanCard -PanasonicDirectory $panasonic } 'Reject invalid Panasonic font size'
    Assert-True (-not (Test-Path -LiteralPath $badPanCard)) 'Validate entire Panasonic batch before writing'
    Remove-Item -LiteralPath (Join-Path $panasonic 'fs-a1f_msx2sub.rom')
    Assert-Rejected { & $importer -Destination $badPanCard -PanasonicDirectory $panasonic -PanasonicModels 'FS-A1F' } 'Reject missing Panasonic component'
    Write-Output "All $checks ROM import checks passed."
}
finally {
    # Only remove the uniquely named fixtures created by this test, leaf first.
    Get-ChildItem -LiteralPath $root -File -Recurse | ForEach-Object { Remove-Item -LiteralPath $_.FullName }
    Get-ChildItem -LiteralPath $root -Directory -Recurse | Sort-Object { $_.FullName.Length } -Descending |
        ForEach-Object { Remove-Item -LiteralPath $_.FullName }
    Remove-Item -LiteralPath $root
}
