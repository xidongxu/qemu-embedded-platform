param(
    [string]$ConfigFile = "configs/qemu.json"
)

$ErrorActionPreference = "Stop"

function Invoke-Git {
    param(
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )
    & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed."
    }
}

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$config = Get-Content (Join-Path $Root $ConfigFile) | ConvertFrom-Json
$Branch = $config.branch
$Commit = $config.commit
$OverlayName = $config.overlay
$Qemu = Join-Path $Root "qemu"
$Patch = Join-Path `
    $Root `
    "patches/qemu/$OverlayName/overlay.patch"

if (!(Test-Path $Patch)) {
    throw "Patch file not found:`n$Patch"
}

Push-Location $Qemu

try {
    Write-Host "Checkout $Commit ..."
    Invoke-Git checkout $Commit
    #
    # 确保工作区干净
    #
    Write-Host "Cleaning qemu..."
    Invoke-Git restore .
    Invoke-Git restore --staged .
    Invoke-Git clean -fd
    #
    # 检查Patch
    #
    Write-Host "Checking patch..."
    Invoke-Git apply --check $Patch
    #
    # 应用Patch
    #
    Write-Host "Applying patch..."
    Invoke-Git apply $Patch

    Write-Host ""
    Write-Host "Patch applied successfully."
}

finally {
    Pop-Location
}
