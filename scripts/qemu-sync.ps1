param(
    [string]$ConfigFile = "configs/qemu.json"
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$config = Get-Content (Join-Path $Root $ConfigFile) | ConvertFrom-Json
$Branch = $config.branch
$Commit = $config.commit
$Qemu = Join-Path $Root "qemu"

Write-Host ""
Write-Host "QEMU sync target:"
Write-Host "Branch : $Branch"
Write-Host "Commit : $Commit"
Write-Host ""

#
# Init submodule
#
Write-Host "Initialize qemu submodule..."
git submodule update --init qemu
if ($LASTEXITCODE -ne 0) {
    throw "Initialize qemu submodule failed."
}
Push-Location $Qemu

#
# Fetch repository
#
Write-Host "Fetch qemu..."
git fetch origin
if ($LASTEXITCODE -ne 0) {
    throw "Fetch qemu failed."
}

#
# Checkout commit
#
Write-Host "Checkout $Commit ..."
git checkout $Commit
if ($LASTEXITCODE -ne 0) {
    throw "Checkout $Commit failed."
}

#
# Clean repository
#
Write-Host "Cleaning qemu ..."
git restore .
git clean -fd

#
# Verify commit
#
$current = git rev-parse HEAD
if ($current.Trim() -ne $Commit) {
    throw "QEMU commit mismatch."
}

Pop-Location

Write-Host ""
Write-Host "QEMU synchronized successfully."
Write-Host ""

git submodule status
