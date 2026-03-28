# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Wavenumber LLC
#
# odbc_monkey ODBC Driver - Uninstall Script
# Removes odbc_monkey ODBC driver registration and any DSNs that reference it.

param(
    [string]$DriverName = "odbc-monkey"
)

$ErrorActionPreference = "Stop"

$legacyDsnNames = @(
    "OdbcMonkey_JSON"
)

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Restart-Elevated {
    param([string]$DriverName)

    Write-Host ""
    Write-Host "Requesting administrator privileges..." -ForegroundColor Yellow

    $argList = @(
        "-ExecutionPolicy", "Bypass",
        "-File", "`"$PSCommandPath`"",
        "-DriverName", "`"$DriverName`""
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
        [string]$KeyName,
        [string]$DriverName
    )

    if (-not $KeyName -or $KeyName -eq "ODBC Drivers") {
        return $false
    }

    if ($KeyName -ieq $DriverName -or $KeyName -ilike "odbc-monkey*") {
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

function Test-IsOdbcMonkeyDsn {
    param(
        [string]$Root,
        [string]$DsnName,
        [string]$DriverName
    )

    if ($legacyDsnNames -icontains $DsnName) {
        return $true
    }

    $dataSourcesKey = Join-Path $Root "ODBC Data Sources"
    if (Test-Path $dataSourcesKey) {
        $dataSourceProps = Get-ItemProperty -Path $dataSourcesKey -ErrorAction SilentlyContinue
        if ($dataSourceProps -and ($dataSourceProps.PSObject.Properties.Name -icontains $DsnName)) {
            $listValue = [string]$dataSourceProps.$DsnName
            if ($listValue -ieq $DriverName) {
                return $true
            }
        }
    }

    $dsnKey = Join-Path $Root $DsnName
    if (-not (Test-Path $dsnKey)) {
        return $false
    }

    $dsnProps = Get-ItemProperty -Path $dsnKey -ErrorAction SilentlyContinue
    if (-not $dsnProps) {
        return $false
    }

    $driverProp = [string]$dsnProps.Driver
    if (-not $driverProp) {
        return $false
    }

    $driverFileName = [System.IO.Path]::GetFileName($driverProp)
    return (
        $driverProp -ieq $DriverName -or
        $driverFileName -ilike "odbc-monkey*.dll" -or
        $driverProp -imatch "odbc-monkey"
    )
}

function Remove-OdbcMonkeyDsns {
    param(
        [string]$Root,
        [string]$Label,
        [string]$DriverName
    )

    if (-not (Test-Path $Root)) {
        return
    }

    $candidateNames = @()
    $dataSourcesKey = Join-Path $Root "ODBC Data Sources"

    $candidateNames += $legacyDsnNames

    if (Test-Path $dataSourcesKey) {
        $dataSourceProps = Get-ItemProperty -Path $dataSourcesKey -ErrorAction SilentlyContinue
        if ($dataSourceProps) {
            $candidateNames += $dataSourceProps.PSObject.Properties |
                Where-Object { $_.MemberType -eq "NoteProperty" -and $_.Name -notlike "PS*" -and ([string]$_.Value) -ieq $DriverName } |
                Select-Object -ExpandProperty Name
        }
    }

    $candidateNames += Get-ChildItem -Path $Root -ErrorAction SilentlyContinue |
        Where-Object { $_.PSChildName -ne "ODBC Data Sources" -and (Test-IsOdbcMonkeyDsn -Root $Root -DsnName $_.PSChildName -DriverName $DriverName) } |
        Select-Object -ExpandProperty PSChildName

    $candidateNames = $candidateNames | Where-Object { $_ } | Sort-Object -Unique

    foreach ($dsnName in $candidateNames) {
        $removedSomething = $false
        $dsnKey = Join-Path $Root $dsnName

        if (Test-Path $dsnKey) {
            Remove-Item -Path $dsnKey -Recurse -Force
            $removedSomething = $true
        }

        if (Test-Path $dataSourcesKey) {
            $valueNames = Get-RegistryValueNames -Path $dataSourcesKey
            if ($valueNames -icontains $dsnName) {
                Remove-ItemProperty -Path $dataSourcesKey -Name $dsnName -ErrorAction SilentlyContinue
                $removedSomething = $true
            }
        }

        if ($removedSomething) {
            Write-Host "  Removed DSN from ${Label}: $dsnName" -ForegroundColor Gray
        }
    }
}

function Remove-DriverRegistrations {
    param(
        [string]$Root,
        [string]$Label,
        [string]$DriverName
    )

    if (-not (Test-Path $Root)) {
        return
    }

    $candidateNames = @($DriverName)
    $candidateNames += Get-ChildItem -Path $Root -ErrorAction SilentlyContinue |
        Where-Object { Test-IsOdbcMonkeyDriverRegistration -Root $Root -KeyName $_.PSChildName -DriverName $DriverName } |
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
            Write-Host "  Removed driver registration from ${Label}: $name" -ForegroundColor Gray
        }
    }
}

function Uninstall-Driver {
    Write-Host ""
    Write-Host "Uninstalling odbc_monkey ODBC Driver..." -ForegroundColor Yellow
    Write-Host ""

    $odbcIniRoots = @(
        @{ Path = "HKLM:\SOFTWARE\ODBC\ODBC.INI"; Label = "system DSN (64-bit)" },
        @{ Path = "HKCU:\SOFTWARE\ODBC\ODBC.INI"; Label = "user DSN" }
    )

    foreach ($root in $odbcIniRoots) {
        Remove-OdbcMonkeyDsns -Root $root.Path -Label $root.Label -DriverName $DriverName
    }

    $odbcInstRoots = @(
        @{ Path = "HKLM:\SOFTWARE\ODBC\ODBCINST.INI"; Label = "driver registry (64-bit)" }
    )

    foreach ($root in $odbcInstRoots) {
        Remove-DriverRegistrations -Root $root.Path -Label $root.Label -DriverName $DriverName
    }

    Write-Host ""
    Write-Host "Uninstallation complete." -ForegroundColor Green
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " odbc_monkey ODBC Driver Cleanup" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

if (-not (Test-Administrator)) {
    Restart-Elevated -DriverName $DriverName
}

Uninstall-Driver
