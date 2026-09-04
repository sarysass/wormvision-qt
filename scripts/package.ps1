param(
    [string]$Version = "1.1.2",
    [string]$MvsRuntimeInstaller = "",
    [string]$MvsRuntimeArgs = "/S",
    [string]$InnoSetupPath = "",
    [string]$BuildDir = "build",
    [string]$VcpkgRoot = "",
    [switch]$UseInstalledDependencies,
    [switch]$SkipBuild,
    [switch]$NoDesktopCopy
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$installerScript = Join-Path $projectRoot "installer\WormVision.iss"
$outputDir = Join-Path $projectRoot "installer\Output"
$desktopDir = [Environment]::GetFolderPath("Desktop")
if (-not [IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $projectRoot $BuildDir
}
$BuildDir = [IO.Path]::GetFullPath($BuildDir)

Set-Location $projectRoot

function Find-InnoCompiler {
    param([string]$ConfiguredPath)

    if ($ConfiguredPath -and (Test-Path $ConfiguredPath)) {
        return $ConfiguredPath
    }

    $candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw "Inno Setup 6 compiler was not found. Install Inno Setup 6 or pass -InnoSetupPath."
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") -BuildDir $BuildDir `
        -VcpkgRoot $VcpkgRoot -UseInstalledDependencies:$UseInstalledDependencies
}

$appExe = Join-Path $BuildDir "WormVision.exe"
if (-not (Test-Path $appExe)) {
    throw "Missing $appExe. Build the Release binary in a Windows/MSVC environment first."
}

if ($MvsRuntimeInstaller -and -not (Test-Path $MvsRuntimeInstaller)) {
    throw "Missing MVS Runtime installer: $MvsRuntimeInstaller"
}

$iscc = Find-InnoCompiler $InnoSetupPath
$compilerArgs = @(
    "/DMyAppVersion=$Version",
    "/DMyBuildDir=$BuildDir",
    "/DMvsRuntimeArgs=$MvsRuntimeArgs"
)

if ($MvsRuntimeInstaller) {
    $compilerArgs += "/DMvsRuntimeInstaller=$((Resolve-Path -LiteralPath $MvsRuntimeInstaller).Path)"
}

$compilerArgs += $installerScript

& $iscc $compilerArgs

if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup packaging failed."
}

$setupPath = Join-Path $outputDir "WormVision-Setup-$Version.exe"
if (-not (Test-Path $setupPath)) {
    throw "Setup file was not found after packaging: $setupPath"
}

Write-Host "Setup generated: $setupPath"
if (-not $NoDesktopCopy -and $desktopDir) {
    $desktopSetupPath = Join-Path $desktopDir "WormVision-Setup-$Version.exe"
    Copy-Item -LiteralPath $setupPath -Destination $desktopSetupPath -Force
    Write-Host "Desktop copy: $desktopSetupPath"
}
