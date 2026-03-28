# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Wavenumber LLC
#
# odbc_monkey ODBC Driver - Install Script
# Registers the native C++ driver from local bin/ and creates a System DSN.

param(
    [string]$JsonDirectory,
    [string]$DsnName = "odbc-monkey"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$binDir = Join-Path $scriptDir "bin"
$driverName = "odbc-monkey"
$driverDescription = "odbc_monkey JSON ODBC Driver for Altium DbLib"
$systemOdbcIniRoot = "HKLM:\SOFTWARE\ODBC\ODBC.INI"
$systemOdbcInstRoot = "HKLM:\SOFTWARE\ODBC\ODBCINST.INI"

if (-not $JsonDirectory) {
    $JsonDirectory = Join-Path $scriptDir "example\json"
}

$driverDll = Get-ChildItem "$binDir\odbc-monkey_*.dll" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $driverDll) {
    Write-Host "ERROR: No driver DLL found in bin/" -ForegroundColor Red
    Write-Host "Run: cd native && .\build.ps1" -ForegroundColor Yellow
    exit 1
}

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Restart-Elevated {
    param(
        [string]$JsonDirectory,
        [string]$DsnName
    )

    Write-Host ""
    Write-Host "Requesting administrator privileges..." -ForegroundColor Yellow

    $argList = @(
        "-ExecutionPolicy", "Bypass",
        "-File", "`"$PSCommandPath`"",
        "-JsonDirectory", "`"$JsonDirectory`"",
        "-DsnName", "`"$DsnName`""
    )

    $proc = Start-Process -FilePath "powershell.exe" -ArgumentList $argList -Verb RunAs -Wait -PassThru
    exit $proc.ExitCode
}

function Get-RegistryValueNames {
    param([string]$Path)

    if (-not (Test-Path $Path)) {
        return @()
    }

    $props = Get-ItemProperty -Path $Path -ErrorAction SilentlyContinue
    if (-not $props) {
        return @()
    }

    return $props.PSObject.Properties |
        Where-Object { $_.MemberType -eq "NoteProperty" -and $_.Name -notlike "PS*" } |
        Select-Object -ExpandProperty Name
}

function Test-IsOdbcMonkeyDriverRegistration {
    param(
        [string]$Root,
        [string]$KeyName
    )

    if (-not $KeyName -or $KeyName -eq "ODBC Drivers") {
        return $false
    }

    if ($KeyName -ilike "odbc-monkey*") {
        return $true
    }

    $driverKey = Join-Path $Root $KeyName
    if (-not (Test-Path $driverKey)) {
        return $false
    }

    $driverProps = Get-ItemProperty -Path $driverKey -ErrorAction SilentlyContinue
    if (-not $driverProps) {
        return $false
    }

    foreach ($value in @([string]$driverProps.Driver, [string]$driverProps.Setup, [string]$driverProps.Description)) {
        if (-not $value) {
            continue
        }

        $fileName = [System.IO.Path]::GetFileName($value)
        if ($fileName -ilike "odbc-monkey*.dll" -or $value -imatch "odbc-monkey") {
            return $true
        }
    }

    return $false
}

function Remove-OdbcMonkeyDriverRegistrations {
    param(
        [string]$Root,
        [string]$Label
    )

    if (-not (Test-Path $Root)) {
        return
    }

    $candidateNames = @($driverName)
    $candidateNames += Get-ChildItem -Path $Root -ErrorAction SilentlyContinue |
        Where-Object { Test-IsOdbcMonkeyDriverRegistration -Root $Root -KeyName $_.PSChildName } |
        Select-Object -ExpandProperty PSChildName

    $candidateNames = $candidateNames | Where-Object { $_ } | Sort-Object -Unique
    $odbcDriversKey = Join-Path $Root "ODBC Drivers"

    foreach ($name in $candidateNames) {
        $removedSomething = $false
        $driverKey = Join-Path $Root $name

        if (Test-Path $driverKey) {
            Remove-Item -Path $driverKey -Recurse -Force
            $removedSomething = $true
        }

        if (Test-Path $odbcDriversKey) {
            $valueNames = Get-RegistryValueNames -Path $odbcDriversKey
            if ($valueNames -icontains $name) {
                Remove-ItemProperty -Path $odbcDriversKey -Name $name -ErrorAction SilentlyContinue
                $removedSomething = $true
            }
        }

        if ($removedSomething) {
            Write-Host "       Removed legacy driver registration from ${Label}: $name" -ForegroundColor Gray
        }
    }
}

function Remove-DsnByName {
    param([string]$Name)

    $odbcIniRoots = @(
        "HKLM:\SOFTWARE\ODBC\ODBC.INI",
        "HKCU:\SOFTWARE\ODBC\ODBC.INI"
    )

    foreach ($root in $odbcIniRoots) {
        $dsnKey = Join-Path $root $Name
        if (Test-Path $dsnKey) {
            Remove-Item -Path $dsnKey -Recurse -Force
        }

        $dataSourcesKey = Join-Path $root "ODBC Data Sources"
        if (Test-Path $dataSourcesKey) {
            $valueNames = Get-RegistryValueNames -Path $dataSourcesKey
            if ($valueNames -icontains $Name) {
                Remove-ItemProperty -Path $dataSourcesKey -Name $Name -ErrorAction SilentlyContinue
            }
        }
    }
}

function Install-Driver {
    Write-Host ""
    Write-Host "Installing odbc_monkey ODBC Driver..." -ForegroundColor Cyan
    Write-Host ""

    Write-Host "[1/2] Registering ODBC driver..." -ForegroundColor Yellow
    $odbcDriversKey = Join-Path $systemOdbcInstRoot "ODBC Drivers"
    $driverKey = Join-Path $systemOdbcInstRoot $driverName

    if (-not (Test-Path $odbcDriversKey)) {
        New-Item -Path $odbcDriversKey -Force | Out-Null
    }

    Remove-OdbcMonkeyDriverRegistrations -Root $systemOdbcInstRoot -Label "driver registry (64-bit)"
    New-Item -Path $driverKey -Force | Out-Null

    Set-ItemProperty -Path $driverKey -Name "Driver" -Value $driverDll.FullName
    Set-ItemProperty -Path $driverKey -Name "Setup" -Value $driverDll.FullName
    Set-ItemProperty -Path $driverKey -Name "Description" -Value $driverDescription
    Set-ItemProperty -Path $driverKey -Name "APILevel" -Value "2"
    Set-ItemProperty -Path $driverKey -Name "ConnectFunctions" -Value "YYN"
    Set-ItemProperty -Path $driverKey -Name "DriverODBCVer" -Value "03.80"
    Set-ItemProperty -Path $driverKey -Name "FileUsage" -Value "0"
    Set-ItemProperty -Path $driverKey -Name "SQLLevel" -Value "1"
    Set-ItemProperty -Path $odbcDriversKey -Name $driverName -Value "Installed"
    Write-Host "       Driver: $($driverDll.Name)" -ForegroundColor Gray

    Write-Host "[2/2] Creating System DSN..." -ForegroundColor Yellow
    Remove-DsnByName -Name $DsnName

    $odbcDataSourcesKey = Join-Path $systemOdbcIniRoot "ODBC Data Sources"
    $dsnKey = Join-Path $systemOdbcIniRoot $DsnName

    if (-not (Test-Path $odbcDataSourcesKey)) {
        New-Item -Path $odbcDataSourcesKey -Force | Out-Null
    }

    New-Item -Path $dsnKey -Force | Out-Null
    Set-ItemProperty -Path $dsnKey -Name "Driver" -Value $driverDll.FullName
    Set-ItemProperty -Path $dsnKey -Name "Description" -Value "odbc_monkey JSON Driver DSN"
    Set-ItemProperty -Path $dsnKey -Name "DataSource" -Value $JsonDirectory
    Set-ItemProperty -Path $odbcDataSourcesKey -Name $DsnName -Value $driverName
    Write-Host "       DSN: $DsnName -> $JsonDirectory" -ForegroundColor Gray

    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host " Installation complete!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Driver:     $driverName"
    Write-Host "DLL:        $($driverDll.FullName)"
    Write-Host "DSN:        $DsnName"
    Write-Host "DataSource: $JsonDirectory"
    Write-Host ""
    Write-Host "Connection strings:"
    Write-Host "  DSN=$DsnName"
    Write-Host "  DRIVER={$driverName};DataSource=$JsonDirectory"
    Write-Host ""
    Write-Host "Verify in ODBC Administrator: odbcad32.exe" -ForegroundColor Gray
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " odbc_monkey ODBC Driver Installer" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

if (-not (Test-Administrator)) {
    Restart-Elevated -JsonDirectory $JsonDirectory -DsnName $DsnName
}

Install-Driver
