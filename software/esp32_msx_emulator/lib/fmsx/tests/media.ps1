param(
  [string]$ProfilesRoot = '',
  [string]$DiskBios = '',
  [ValidateRange(1,36000)][int]$Frames = 900
)
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot\host_smoke.ps1" -MediaOnly -ProfilesRoot $ProfilesRoot -DiskBios $DiskBios -Frames $Frames
