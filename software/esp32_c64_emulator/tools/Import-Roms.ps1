param(
    [Parameter(Mandatory = $true)][string]$BasicRom,
    [Parameter(Mandatory = $true)][string]$KernalRom,
    [Parameter(Mandatory = $true)][string]$CharacterRom,
    [string]$Destination = (Join-Path (Split-Path -Parent $PSScriptRoot) 'sdcard')
)
$ErrorActionPreference = 'Stop'
$entries = @(
    @{ Source = $BasicRom; Name = 'basic.rom'; Size = 8192 },
    @{ Source = $KernalRom; Name = 'kernal.rom'; Size = 8192 },
    @{ Source = $CharacterRom; Name = 'chargen.rom'; Size = 4096 }
)
foreach ($entry in $entries) {
    $source = Get-Item -LiteralPath $entry.Source -ErrorAction Stop
    if ($source.PSIsContainer -or $source.Length -ne $entry.Size) {
        throw "$($entry.Name) must be exactly $($entry.Size) bytes; padded EPROM images are not accepted."
    }
}
$target = Join-Path ([IO.Path]::GetFullPath($Destination)) 'c64\bios'
New-Item -ItemType Directory -Path $target -Force | Out-Null
foreach ($entry in $entries) {
    $output = Join-Path $target $entry.Name
    if ([IO.Path]::GetFullPath($entry.Source) -ne [IO.Path]::GetFullPath($output)) {
        Copy-Item -LiteralPath $entry.Source -Destination $output -Force
    }
    if ((Get-FileHash -LiteralPath $entry.Source -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash) {
        throw "ROM copy verification failed: $output"
    }
    Write-Output "Verified $output ($($entry.Size) bytes)."
}
