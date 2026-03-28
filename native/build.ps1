# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Wavenumber LLC
#
# Build Script for odbc-monkey (CMake-based)
#
# Options:
#   -Clean      Clean build directory before building
#   -Test       Run tests after building (auto-installs driver)
#   -Release    Release build (default)
#   -Debug      Debug build
#
# Examples:
#   .\build.ps1              # Build release
#   .\build.ps1 -Test        # Build, install driver, and test
#   .\build.ps1 -Clean       # Clean rebuild

param(
    [switch]$Clean,
    [switch]$Test,
    [switch]$Debug
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmakeBuildDir = Join-Path $scriptDir "cmake-build"
$buildDir = Join-Path $scriptDir "build"
$binDir = Join-Path (Split-Path $scriptDir -Parent) "bin"

$buildType = if ($Debug) { "Debug" } else { "Release" }

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " odbc-monkey Build Script (CMake)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check for CMake
$cmakePath = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakePath) {
    Write-Host "ERROR: CMake not found in PATH!" -ForegroundColor Red
    Write-Host "Install CMake from https://cmake.org/download/" -ForegroundColor Yellow
    exit 1
}
Write-Host "CMake: $($cmakePath.Source)" -ForegroundColor Gray

# Run clang-format if available
$clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue
if ($clangFormat) {
    Write-Host ""
    Write-Host "[Format] Running clang-format..." -ForegroundColor Yellow
    $srcFiles = Get-ChildItem -Path "$scriptDir\src\*.cpp", "$scriptDir\include\*.h" -ErrorAction SilentlyContinue
    foreach ($file in $srcFiles) {
        & clang-format -i $file.FullName
    }
    Write-Host "         Formatted $($srcFiles.Count) files" -ForegroundColor Gray
}

# Clean build directory if requested
if ($Clean) {
    Write-Host "[Clean] Removing build directories..." -ForegroundColor Yellow
    if (Test-Path $cmakeBuildDir) {
        Remove-Item -Path $cmakeBuildDir -Recurse -Force
    }
    if (Test-Path $buildDir) {
        Remove-Item -Path $buildDir -Recurse -Force
    }
    Write-Host ""
}

# Create build directory
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
}

# Find Visual Studio for MSVC
$vsPaths = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Community",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\Enterprise",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
)

$vsPath = $null
foreach ($p in $vsPaths) {
    if (Test-Path $p) {
        $vsPath = $p
        break
    }
}

if (-not $vsPath) {
    Write-Host "ERROR: Visual Studio 2022 not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Run setup_build_tools.ps1 to install build tools:" -ForegroundColor Yellow
    Write-Host "  .\setup_build_tools.ps1" -ForegroundColor Gray
    exit 1
}
Write-Host "VS2022: $vsPath" -ForegroundColor Gray

# Configure - use Visual Studio generator (more reliable than Ninja+MSVC detection)
Write-Host ""
Write-Host "[CMake] Configuring..." -ForegroundColor Yellow

$cmakeArgs = @(
    "-G", "Visual Studio 17 2022"
    "-A", "x64"
    "-B", $cmakeBuildDir
)

Push-Location $scriptDir
try {
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "      CMake configure FAILED!" -ForegroundColor Red
        exit 1
    }

    # Build
    Write-Host ""
    Write-Host "[CMake] Building ($buildType)..." -ForegroundColor Yellow
    & cmake --build $cmakeBuildDir --config $buildType --parallel
    if ($LASTEXITCODE -ne 0) {
        Write-Host "      CMake build FAILED!" -ForegroundColor Red
        exit 1
    }

    $versionString = "unknown"
    $builtDll = Get-ChildItem "$binDir\odbc-monkey_*.dll" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($builtDll -and $builtDll.BaseName -match "^odbc-monkey_(.+)$") {
        $versionString = $Matches[1]
    }

    Write-Host ""
    Write-Host "[Build] Build succeeded (version: $versionString)" -ForegroundColor Green
    if ($builtDll) {
        Write-Host "        DLL: $($builtDll.FullName)" -ForegroundColor Gray
    } else {
        Write-Host "        DLL: $binDir\odbc-monkey_$versionString.dll" -ForegroundColor Gray
    }
    Write-Host ""

} finally {
    Pop-Location
}

# Run tests
if ($Test) {
    # Auto-install driver before running tests
    $installScript = Join-Path (Split-Path $scriptDir -Parent) "install.ps1"
    Write-Host "[Install] Registering driver before tests..." -ForegroundColor Yellow
    & powershell -ExecutionPolicy Bypass -File $installScript
    if ($LASTEXITCODE -ne 0) {
        Write-Host "      Driver installation failed" -ForegroundColor Red
        exit 1
    }
    Write-Host ""
    Write-Host "[Test] Running ODBC test suite..." -ForegroundColor Yellow
    Write-Host ""

    # VS generator puts exe in Release/ or Debug/ subfolder
    $testExe = Join-Path $cmakeBuildDir "$buildType\odbc_test.exe"
    if (-not (Test-Path $testExe)) {
        # Fallback for Ninja generator
        $testExe = Join-Path $cmakeBuildDir "odbc_test.exe"
    }
    if (-not (Test-Path $testExe)) {
        Write-Host "      ERROR: Test executable not found" -ForegroundColor Red
        exit 1
    }

    & $testExe

    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "      Some tests FAILED!" -ForegroundColor Red
    } else {
        Write-Host ""
        Write-Host "      All tests PASSED!" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "Done!" -ForegroundColor Green
