param([string]$RomDirectory = '')
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'Test-Core.ps1') -RomSmoke -RomDirectory $RomDirectory
