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
    # 保留构建目录(qemu-configure/qemu-build)，实现增量构建，避免每次全量编译。
    # 直接调用 git（不走 Invoke-Git），避免 PowerShell 把 -e 当作参数名解析。
    & git clean -fd -e qemu-configure -e qemu-build
    if ($LASTEXITCODE -ne 0) {
        throw "git clean -fd -e qemu-configure -e qemu-build failed."
    }
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
