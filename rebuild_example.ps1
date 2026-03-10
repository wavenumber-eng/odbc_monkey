# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Wavenumber LLC
#
# rebuild_example.ps1
# Generates example.DbLib from the example JSON files
#
# Usage:
#   .\rebuild_example.ps1
#
# Prerequisites:
#   - Python 3.x with uv (or standard Python)
#   - odbc-monkey driver installed (for Altium to use the generated DbLib)

$ErrorActionPreference = "Stop"

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ExampleDir = Join-Path $ScriptDir "example"
$JsonDir = Join-Path $ExampleDir "json"

Write-Host "OdbcMonkey Example DbLib Builder" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host ""

# Check if example directory exists
if (-not (Test-Path $ExampleDir)) {
    Write-Host "ERROR: Example directory not found: $ExampleDir" -ForegroundColor Red
    exit 1
}

# Check if json directory exists and has files
if (-not (Test-Path $JsonDir)) {
    Write-Host "ERROR: JSON directory not found: $JsonDir" -ForegroundColor Red
    exit 1
}

$JsonFiles = Get-ChildItem -Path $JsonDir -Filter "*.json"
if ($JsonFiles.Count -eq 0) {
    Write-Host "ERROR: No JSON files found in: $JsonDir" -ForegroundColor Red
    exit 1
}

Write-Host "JSON directory: $JsonDir" -ForegroundColor Gray
Write-Host "Found $($JsonFiles.Count) JSON file(s)" -ForegroundColor Gray
Write-Host ""

# Run dblib_builder.py
$DbLibBuilder = Join-Path $ScriptDir "dblib_builder.py"

if (-not (Test-Path $DbLibBuilder)) {
    Write-Host "ERROR: dblib_builder.py not found: $DbLibBuilder" -ForegroundColor Red
    exit 1
}

Write-Host "Generating example.DbLib..." -ForegroundColor Yellow

# Try uv first, fall back to python
$UvPath = Get-Command uv -ErrorAction SilentlyContinue
if ($UvPath) {
    & uv run python $DbLibBuilder --json-path $JsonDir --output $ExampleDir --name "example" --verbose
} else {
    & python $DbLibBuilder --json-path $JsonDir --output $ExampleDir --name "example" --verbose
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to generate DbLib" -ForegroundColor Red
    exit 1
}

$DbLibPath = Join-Path $ExampleDir "example.DbLib"
if (Test-Path $DbLibPath) {
    Write-Host ""
    Write-Host "Success!" -ForegroundColor Green
    Write-Host "Generated: $DbLibPath" -ForegroundColor Green
    Write-Host ""
    Write-Host "Example structure:" -ForegroundColor Cyan
    Write-Host "  example/" -ForegroundColor Gray
    Write-Host "    example.DbLib      <- Generated DbLib file" -ForegroundColor White
    Write-Host "    symbols/" -ForegroundColor Gray
    Write-Host "      R_2P.SchLib      <- Resistor symbol" -ForegroundColor Gray
    Write-Host "      C_2P_NP.SchLib   <- Capacitor symbol" -ForegroundColor Gray
    Write-Host "    footprints/" -ForegroundColor Gray
    Write-Host "      R0603_HD.PcbLib  <- Resistor footprint" -ForegroundColor Gray
    Write-Host "      C0603_HD.PcbLib  <- Capacitor footprint" -ForegroundColor Gray
    Write-Host "    json/" -ForegroundColor Gray
    foreach ($file in $JsonFiles) {
        Write-Host "      $($file.Name)" -ForegroundColor Gray
    }
    Write-Host ""
    Write-Host "To use in Altium:" -ForegroundColor Cyan
    Write-Host "  1. Ensure odbc-monkey driver is installed" -ForegroundColor Gray
    Write-Host "  2. Open example.DbLib in Altium" -ForegroundColor Gray
    Write-Host "  3. Components will appear in the database library panel" -ForegroundColor Gray
} else {
    Write-Host "ERROR: DbLib file was not created" -ForegroundColor Red
    exit 1
}
