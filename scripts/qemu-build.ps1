<#
.SYNOPSIS
    One-step QEMU patch + MSYS2 MINGW64 build (admin).

.DESCRIPTION
    1. Apply the overlay patch (qemu-apply-patch.ps1)
    2. Build in MSYS2 MINGW64

    Admin is required because QEMU configure creates symlinks(scripts/symlink-install-tree.py) on Windows.

.PARAMETER ConfigFile
    Config file path (relative to repo root). Default: configs/qemu.json

.PARAMETER Msys2Root
    MSYS2 root dir. An explicit value is used directly (errors if invalid);
    otherwise probed in order: 
    1. $env:USERPROFILE\program\MSYS64
    2. C:\msys64 or C:\msys2
    3. registry.

.PARAMETER SkipPatch
    Skip the patch step, only build.

.PARAMETER SkipAdmin
    Do not elevate (only if Windows Developer Mode is enabled).

.EXAMPLE
    .\scripts\qemu-build.ps1
#>
[CmdletBinding()]
param(
    [string]$ConfigFile = "configs/qemu.json",
    [string]$Msys2Root = "$env:USERPROFILE\program\MSYS64",
    [switch]$SkipPatch,
    [switch]$SkipAdmin
)

$ErrorActionPreference = "Stop"

# Repo root (this script lives in <root>\scripts\)
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

function Get-Msys2Root {
    param(
        [string]$Default,
        [switch]$ExplicitPassed
    )
    # 1) Explicit -Msys2Root: use directly, error if invalid
    if ($ExplicitPassed) {
        if (Test-Path (Join-Path $Default "msys2_shell.cmd")) { return $Default }
        throw "Invalid -Msys2Root path: $Default"
    }
    # 2) No explicit value: try the default first
    if (Test-Path (Join-Path $Default "msys2_shell.cmd")) { return $Default }
    # 3) Try standard install locations
    foreach ($p in @("C:\msys64", "C:\msys2")) {
        if (Test-Path (Join-Path $p "msys2_shell.cmd")) { return $p }
    }
    # 4) Fall back to the registry (MSYS2 uninstall entry)
    $uninstallKeys = @(
        "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*"
    )
    $entry = Get-ItemProperty $uninstallKeys -ErrorAction SilentlyContinue |
        Where-Object { $_.DisplayName -eq "MSYS2" -and $_.InstallLocation } |
        Select-Object -First 1
    if ($entry) {
        $candidate = $entry.InstallLocation
        if (Test-Path (Join-Path $candidate "msys2_shell.cmd")) { return $candidate }
    }
    return $null
}

function ConvertTo-PosixPath {
    param([string]$WindowsPath)
    # C:\foo\bar -> /c/foo/bar (MSYS2 mounts use lowercase drive letters)
    $drive = $WindowsPath.Substring(0, 1).ToLowerInvariant()
    $rest = $WindowsPath.Substring(2) -replace '\\', '/'
    return "/$drive$rest"
}

# Run the build in MSYS2 MINGW64 in the same (elevated) console via a temp .cmd.
function Invoke-Msys2Build {
    param(
        [string]$Msys2Shell,
        [string]$BuildCmd,
        [switch]$AsAdmin
    )
    $tmpCmd = Join-Path $env:TEMP ("qemu-build-" + [guid]::NewGuid().ToString("N") + ".cmd")
    $batch = "@echo off`r`n" +
             "call `"$Msys2Shell`" -mingw64 -defterm -no-start -c `"$BuildCmd`"`r`n" +
             "exit /b %ERRORLEVEL%`r`n"
    Set-Content -Path $tmpCmd -Value $batch -Encoding Ascii
    try {
        if ($AsAdmin) {
            $proc = Start-Process -FilePath $tmpCmd -Verb RunAs -Wait -PassThru
            return $proc.ExitCode
        }
        else {
            & $tmpCmd
            return $LASTEXITCODE
        }
    }
    finally {
        Remove-Item -Path $tmpCmd -Force -ErrorAction SilentlyContinue
    }
}

# Step 1: apply patch
if (-not $SkipPatch) {
    Write-Host ""
    Write-Host "Applying patch..."
    & (Join-Path $Root "scripts\qemu-apply-patch.ps1") -ConfigFile $ConfigFile
    if ($LASTEXITCODE -ne 0) {
        throw "Patch failed (exit $LASTEXITCODE)"
    }
}
else {
    Write-Host ""
    Write-Host "Skipping patch (-SkipPatch)"
}

# Step 2: build in MSYS2 MINGW64
$msys2Root = Get-Msys2Root -Default $Msys2Root -ExplicitPassed $PSBoundParameters.ContainsKey('Msys2Root')
if (-not $msys2Root) {
    throw "MSYS2 not found. Specify it with -Msys2Root, e.g. -Msys2Root C:\msys64"
}
$msys2Shell = Join-Path $msys2Root "msys2_shell.cmd"
$posix = ConvertTo-PosixPath $Root
$buildCmd = "cd '$posix' && ./scripts/qemu-build-msys2.sh"

Write-Host ""
Write-Host "Building QEMU in MSYS2 MINGW64..."
Write-Host "MSYS2 root : $msys2Root"

if (-not $SkipAdmin) {
    Write-Host "Requesting admin rights (accept the UAC prompt)..."
    try {
        $code = Invoke-Msys2Build -Msys2Shell $msys2Shell -BuildCmd $buildCmd -AsAdmin
        if ($code -ne 0) {
            throw "Build failed (exit $code)"
        }
    }
    catch {
        if ($_.Exception.Message -match "canceled|cancelled|denied") {
            Write-Warning "Elevation canceled, build aborted."
        }
        else { throw }
    }
}
else {
    Write-Host "Running without elevation (-SkipAdmin)"
    $code = Invoke-Msys2Build -Msys2Shell $msys2Shell -BuildCmd $buildCmd
    if ($code -ne 0) {
        throw "Build failed (exit $code)"
    }
}

Write-Host ""
Write-Host "Done."



