param(
    [string]$ConfigFile = "configs/qemu.json"
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$config = Get-Content (Join-Path $Root $ConfigFile) | ConvertFrom-Json
$Branch = $config.branch
$Commit = $config.commit
$OverlayName = $config.overlay
$Qemu = Join-Path $Root "qemu"
$Overlay = Join-Path $Root "overlay/qemu/$OverlayName"
$PatchDir = Join-Path $Root "patches/qemu/$OverlayName"

if (!(Test-Path $PatchDir)) {
    New-Item -ItemType Directory -Path $PatchDir | Out-Null
}
$PatchFile = Join-Path $PatchDir "overlay.patch"
Push-Location $Qemu

Write-Host "Checkout $Commit ..."
git checkout $Commit

Write-Host "Cleaning qemu ..."
git restore .
git clean -fd

Write-Host "Copy overlay..."
Copy-Item "$Overlay\*" $Qemu -Recurse -Force

Write-Host "Checking diff..."
git diff --cached --check
if ($LASTEXITCODE -ne 0) {
    throw "Whitespace errors detected."
}

Write-Host "Exporting patch..."
git add -N .
cmd /c "git diff --binary > `"$PatchFile`""

Write-Host "Restore qemu..."
git restore .
git restore --staged .
git clean -fd

Pop-Location
Write-Host ""
Write-Host "Patch generated:"
Write-Host $PatchFile
