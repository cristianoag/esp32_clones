[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Destination,
    [string]$OmegaRom,
    [ValidateSet(0, 1)][int]$OmegaBank = 0,
    [string]$ExpertRom,
    [string]$HotbitRom,
    [string]$PanasonicDirectory,
    [ValidateSet('FS-A1WSX', 'FS-A1F', 'FS-A1FX')][string[]]$PanasonicModels = @('FS-A1WSX', 'FS-A1F', 'FS-A1FX'),
    [string]$BiosRom,
    [string]$ExtensionRom,
    [ValidatePattern('^[a-zA-Z0-9_-]{1,32}$')][string]$ProfileId,
    [string]$Name,
    [ValidateSet('MSX1', 'MSX2', 'MSX2+')][string]$Model = 'MSX1',
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$profiles = [System.Collections.Generic.List[object]]::new()

function Read-SizedRom([string]$Path, [int[]]$Sizes) {
    $resolved = (Resolve-Path -LiteralPath $Path).ProviderPath
    $file = Get-Item -LiteralPath $resolved
    if ($file.PSIsContainer -or $file.Length -notin $Sizes) {
        throw "Invalid ROM size for '$Path': $($file.Length) bytes; expected $($Sizes -join ' or ')."
    }
    $bytes = [System.IO.File]::ReadAllBytes($resolved)
    if ($bytes.Length -notin $Sizes) { throw "ROM size changed while reading '$Path'." }
    return ,$bytes
}

function Add-Profile([string]$Id, [string]$Title, [string]$Machine,
                     [byte[]]$Main, [byte[]]$Extension, [string]$Source, [hashtable]$Files) {
    if ([string]::IsNullOrWhiteSpace($Title) -or $Title.Length -gt 40 -or $Title -match '[^\x20-\x7e]') {
        throw 'Profile name must be 1-40 printable ASCII characters.'
    }
    if (-not $Files) {
        $Files = @{}
        switch ($Machine) {
            'MSX1'  { $Files['MSX.ROM'] = $Main }
            'MSX2'  { $Files['MSX2.ROM'] = $Main; $Files['MSX2EXT.ROM'] = $Extension }
            'MSX2+' { $Files['MSX2P.ROM'] = $Main; $Files['MSX2PEXT.ROM'] = $Extension }
        }
    }
    $ram = if ($Machine -eq 'MSX1') { 64 } else { 512 }
    $profiles.Add(@{ Id = $Id; Title = $Title; Model = $Machine; Ram = $ram; Files = $files; Source = $Source })
}

if ($OmegaRom) {
    $omega = Read-SizedRom $OmegaRom @(262144, 524288)
    if ($omega.Length -eq 262144 -and $OmegaBank -ne 0) {
        throw 'A 256 KiB Omega image has only bank 0.'
    }
    $offset = $OmegaBank * 262144
    $bank = [byte[]]::new(262144)
    [Array]::Copy($omega, $offset, $bank, 0, $bank.Length)
    Add-Profile 'omega' "Omega MSX2+ NTSC (bank $OmegaBank)" 'MSX2+' $null $null `
        "Omega bank $OmegaBank; complete 256 KiB image, including BIOS, logo and extension" @{'OMEGA.ROM' = $bank}
}
if ($ExpertRom) {
    Add-Profile 'expert' 'Gradiente Expert 1.1' 'MSX1' (Read-SizedRom $ExpertRom @(32768)) $null 'User-supplied 32 KiB BIOS'
}
if ($HotbitRom) {
    Add-Profile 'hotbit' 'Sharp Hotbit 1.2' 'MSX1' (Read-SizedRom $HotbitRom @(32768)) $null 'User-supplied 32 KiB BIOS'
}
if ($PanasonicDirectory) {
    $panasonicRoot = (Resolve-Path -LiteralPath $PanasonicDirectory).ProviderPath
    foreach ($machineName in ($PanasonicModels | Select-Object -Unique)) {
        $id = $machineName.ToLowerInvariant()
        $isMsx2 = $machineName -eq 'FS-A1F'
        $biosSuffix = if ($isMsx2) { 'basic-bios2.rom' } else { 'basic-bios2p.rom' }
        $subSuffix = if ($isMsx2) { 'msx2sub.rom' } else { 'msx2psub.rom' }
        $fontSize = if ($machineName -eq 'FS-A1WSX') { 262144 } else { 131072 }
        $main = Read-SizedRom (Join-Path $panasonicRoot "$id`_$biosSuffix") @(32768)
        $extension = Read-SizedRom (Join-Path $panasonicRoot "$id`_$subSuffix") @(16384)
        $kanjiBasic = Read-SizedRom (Join-Path $panasonicRoot "$id`_kanjibasic.rom") @(32768)
        $kanjiFont = Read-SizedRom (Join-Path $panasonicRoot "$id`_kanjifont.rom") @($fontSize)
        $image = [byte[]]::new(0x14000 + $fontSize)
        [Array]::Copy($main, 0, $image, 0, $main.Length)
        [Array]::Copy($extension, 0, $image, 0x8000, $extension.Length)
        [Array]::Copy($kanjiBasic, 0, $image, 0xC000, $kanjiBasic.Length)
        [Array]::Copy($kanjiFont, 0, $image, 0x14000, $kanjiFont.Length)
        $machineModel = if ($isMsx2) { 'MSX2' } else { 'MSX2+' }
        Add-Profile $id "Panasonic $machineName" $machineModel $null $null `
            'Combined local BIOS, sub-ROM, Kanji BASIC and Kanji font; no disk/app ROMs' @{'PANASONIC.ROM' = $image}
        $profiles[$profiles.Count - 1].Ram = 64
    }
}
if ($BiosRom) {
    if (-not $ProfileId -or -not $Name) {
        throw 'Custom BIOS import requires -ProfileId and -Name.'
    }
    if ($ProfileId -in @('omega', 'expert', 'hotbit', 'fs-a1wsx', 'fs-a1f', 'fs-a1fx')) {
        throw 'Custom profile ID is reserved for a built-in import profile.'
    }
    $main = Read-SizedRom $BiosRom @(32768)
    $extension = $null
    if ($Model -ne 'MSX1') {
        if (-not $ExtensionRom) { throw 'MSX2/MSX2+ import requires a 16 KiB -ExtensionRom.' }
        $extension = Read-SizedRom $ExtensionRom @(16384)
    } elseif ($ExtensionRom) {
        throw 'An MSX1 profile does not use an extension ROM.'
    }
    Add-Profile $ProfileId $Name $Model $main $extension 'User-supplied BIOS and extension'
}
if ($profiles.Count -eq 0) { throw 'Specify at least one ROM to import.' }

$root = Join-Path ([System.IO.Path]::GetFullPath($Destination)) 'msx\bios'
# Validate every input and destination before replacing any existing profile.
foreach ($profile in $profiles) {
    $target = Join-Path $root $profile.Id
    if ((Test-Path -LiteralPath $target) -and -not $Force) {
        throw "Profile '$($profile.Id)' already exists; use -Force to replace its imported files."
    }
}
foreach ($profile in $profiles) {
    $target = Join-Path $root $profile.Id
    [System.IO.Directory]::CreateDirectory($target) | Out-Null
    foreach ($entry in $profile.Files.GetEnumerator()) {
        [System.IO.File]::WriteAllBytes((Join-Path $target $entry.Key), $entry.Value)
    }
    $manifest = "name=$($profile.Title)`nmodel=$($profile.Model)`nram=$($profile.Ram)`n"
    [System.IO.File]::WriteAllText((Join-Path $target 'profile.ini'), $manifest, [System.Text.Encoding]::ASCII)
    $checksums = foreach ($filename in ($profile.Files.Keys | Sort-Object)) {
        $hash = (Get-FileHash -LiteralPath (Join-Path $target $filename) -Algorithm SHA256).Hash
        "$hash  $filename"
    }
    [System.IO.File]::WriteAllLines((Join-Path $target 'SHA256SUMS.txt'), $checksums, [System.Text.Encoding]::ASCII)
    if ($profile.Files.ContainsKey('OMEGA.ROM')) {
        # Migrate only the known split files created by the earlier importer.
        foreach ($legacy in @('MSX2P.ROM', 'MSX2PEXT.ROM', 'MSX2PLOGO.ROM')) {
            $legacyPath = Join-Path $target $legacy
            if ([System.IO.File]::Exists($legacyPath)) { [System.IO.File]::Delete($legacyPath) }
        }
    }
    Write-Output "Imported $($profile.Title) to $target ($($profile.Source))."
}
