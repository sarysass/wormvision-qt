param(
    [string]$VcpkgRoot = "",
    [string]$BuildDir = "build",
    [string]$MvsRuntimeDir = "",
    [string]$MvsLibDir = "",
    [string]$CMakePath = "",
    [string]$NinjaPath = "",
    [switch]$UseInstalledDependencies
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "build-common.ps1")

$build = Initialize-WormVisionBuild @PSBoundParameters
Invoke-WormVisionConfigure $build
Write-Host "CMake 配置完成：$($build.BuildDir)"
