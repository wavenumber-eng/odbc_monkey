# setup_build_tools.ps1
# Installs build tools for odbc-monkey
#
# Usage:
#   .\setup_build_tools.ps1              # Install Visual Studio Build Tools + clang-format (default)
#   .\setup_build_tools.ps1 -CMake       # Install CMake + Ninja + clang-format for manual CMake workflows
#   .\setup_build_tools.ps1 -Clang       # Install Clang/LLVM + CMake + Ninja for alternative local toolchains
#   .\setup_build_tools.ps1 -All         # Install everything

param(
    [switch]$CMake,
    [switch]$Clang,
    [switch]$All,
    [switch]$Help
)

if ($Help) {
    Write-Host @"
odbc-monkey Build Tools Setup
==================================

Usage:
    .\setup_build_tools.ps1              Install Visual Studio Build Tools + clang-format (default, recommended)
    .\setup_build_tools.ps1 -CMake       Install CMake + Ninja + clang-format for manual CMake workflows
    .\setup_build_tools.ps1 -Clang       Install Clang/LLVM + CMake + Ninja for alternative local toolchains
    .\setup_build_tools.ps1 -All         Install everything
    .\setup_build_tools.ps1 -Help        Show this help

Build options:
    .\build.ps1                          Release build
    .\build.ps1 -Test                    Build, install driver, and run tests
    .\build.ps1 -Debug                   Debug build
    .\build.ps1 -Clean                   Clean rebuild

No external dependencies - everything is in the repo!
"@
    exit 0
}

$ErrorActionPreference = "Stop"

function Test-Admin {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Install-WithWinget {
    param([string]$PackageId, [string]$Name)

    Write-Host "Installing $Name..." -ForegroundColor Cyan
    $result = winget list --id $PackageId 2>$null
    if ($LASTEXITCODE -eq 0 -and $result -match $PackageId) {
        Write-Host "  $Name is already installed" -ForegroundColor Green
        return $true
    }

    winget install --id $PackageId --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  Failed to install $Name" -ForegroundColor Red
        return $false
    }
    Write-Host "  $Name installed successfully" -ForegroundColor Green
    return $true
}

function Install-ClangFormat {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Yellow
    Write-Host "Installing clang-format" -ForegroundColor Yellow
    Write-Host "========================================" -ForegroundColor Yellow
    Write-Host ""

    $clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($clangFormat) {
        Write-Host "clang-format already installed at: $($clangFormat.Source)" -ForegroundColor Green
        return $true
    }

    $wingetExists = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $wingetExists) {
        Write-Host "winget not found. Cannot install clang-format automatically." -ForegroundColor Red
        Write-Host "Install LLVM manually or rerun with winget available." -ForegroundColor Yellow
        return $false
    }

    # LLVM package includes clang-format and puts it on PATH.
    return Install-WithWinget "LLVM.LLVM" "LLVM (clang-format)"
}

function Install-VisualStudioBuildTools {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Yellow
    Write-Host "Installing Visual Studio Build Tools" -ForegroundColor Yellow
    Write-Host "========================================" -ForegroundColor Yellow
    Write-Host ""

    # Check if already installed
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($vsPath) {
            Write-Host "Visual Studio Build Tools already installed at: $vsPath" -ForegroundColor Green
            return $true
        }
    }

    Write-Host "Downloading Visual Studio Build Tools installer..." -ForegroundColor Cyan

    $installerUrl = "https://aka.ms/vs/17/release/vs_buildtools.exe"
    $installerPath = Join-Path $env:TEMP "vs_buildtools.exe"

    try {
        Invoke-WebRequest -Uri $installerUrl -OutFile $installerPath -UseBasicParsing
    } catch {
        Write-Host "Failed to download installer: $_" -ForegroundColor Red
        return $false
    }

    Write-Host "Installing Visual Studio Build Tools (this may take 10-30 minutes)..." -ForegroundColor Cyan
    Write-Host "Components: MSVC, Windows SDK, CMake" -ForegroundColor Gray

    # Install with required components for C++ development
    $args = @(
        "--quiet"
        "--wait"
        "--norestart"
        "--nocache"
        "--add", "Microsoft.VisualStudio.Workload.VCTools"
        "--add", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
        "--add", "Microsoft.VisualStudio.Component.Windows11SDK.22621"
        "--add", "Microsoft.VisualStudio.Component.VC.CMake.Project"
        "--includeRecommended"
    )

    $process = Start-Process -FilePath $installerPath -ArgumentList $args -Wait -PassThru

    Remove-Item $installerPath -Force -ErrorAction SilentlyContinue

    if ($process.ExitCode -eq 0 -or $process.ExitCode -eq 3010) {
        Write-Host "Visual Studio Build Tools installed successfully!" -ForegroundColor Green
        if ($process.ExitCode -eq 3010) {
            Write-Host "  Note: A restart may be required" -ForegroundColor Yellow
        }
        return $true
    } else {
        Write-Host "Installation failed with exit code: $($process.ExitCode)" -ForegroundColor Red
        return $false
    }
}

function Install-CMakeNinja {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Yellow
    Write-Host "Installing CMake + Ninja + ClangFormat" -ForegroundColor Yellow
    Write-Host "========================================" -ForegroundColor Yellow
    Write-Host ""

    # Check for winget
    $wingetExists = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $wingetExists) {
        Write-Host "winget not found. Please install it from the Microsoft Store (App Installer)" -ForegroundColor Red
        return $false
    }

    $success = $true

    # Install CMake
    if (-not (Install-WithWinget "Kitware.CMake" "CMake")) {
        $success = $false
    }

    # Install Ninja
    if (-not (Install-WithWinget "Ninja-build.Ninja" "Ninja")) {
        $success = $false
    }

    # Install LLVM (includes clang-format)
    if (-not (Install-WithWinget "LLVM.LLVM" "LLVM (clang-format)")) {
        $success = $false
    }

    return $success
}

function Install-ClangToolchain {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Yellow
    Write-Host "Installing Clang/LLVM Toolchain" -ForegroundColor Yellow
    Write-Host "========================================" -ForegroundColor Yellow
    Write-Host ""

    # Check for winget
    $wingetExists = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $wingetExists) {
        Write-Host "winget not found. Please install it from the Microsoft Store (App Installer)" -ForegroundColor Red
        return $false
    }

    $success = $true

    # Install LLVM/Clang
    if (-not (Install-WithWinget "LLVM.LLVM" "LLVM/Clang")) {
        $success = $false
    }

    # Install CMake + Ninja
    if (-not (Install-CMakeNinja)) {
        $success = $false
    }

    if ($success) {
        Write-Host ""
        Write-Host "Clang toolchain installed successfully!" -ForegroundColor Green
    }

    return $success
}

# Main execution
Write-Host ""
Write-Host "odbc-monkey Build Tools Setup" -ForegroundColor Magenta
Write-Host "===================================" -ForegroundColor Magenta
Write-Host ""

# Warn if not admin (some installs may fail)
if (-not (Test-Admin)) {
    Write-Host "Note: Running without admin privileges." -ForegroundColor Yellow
    Write-Host ""
}

$overallSuccess = $true

# Install based on options
if ($All) {
    # Install everything
    if (-not (Install-VisualStudioBuildTools)) { $overallSuccess = $false }
    if (-not (Install-ClangToolchain)) { $overallSuccess = $false }
} elseif ($Clang) {
    # Install Clang + CMake + Ninja
    if (-not (Install-ClangToolchain)) { $overallSuccess = $false }
} elseif ($CMake) {
    # Install CMake + Ninja only (assumes MSVC is already installed)
    if (-not (Install-CMakeNinja)) { $overallSuccess = $false }
} else {
    # Default: Install Visual Studio Build Tools + clang-format
    if (-not (Install-VisualStudioBuildTools)) { $overallSuccess = $false }
    if (-not (Install-ClangFormat)) { $overallSuccess = $false }
}

# Summary
Write-Host ""
Write-Host "========================================" -ForegroundColor Yellow
Write-Host "Setup Summary" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Yellow
Write-Host ""

if ($overallSuccess) {
    Write-Host "All tools installed successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "  1. Open a new terminal (to refresh PATH)" -ForegroundColor White
    Write-Host "  2. Run: .\build.ps1 -Test" -ForegroundColor White
    Write-Host ""
    if ($CMake -or $Clang -or $All) {
        Write-Host "Optional note:" -ForegroundColor Cyan
        Write-Host "  The extra CMake/Ninja/Clang tools are for manual local workflows." -ForegroundColor White
        Write-Host "  The repo's supported build script is still .\build.ps1." -ForegroundColor White
        Write-Host ""
    }
} else {
    Write-Host "Some installations failed. Check the output above." -ForegroundColor Red
    exit 1
}
