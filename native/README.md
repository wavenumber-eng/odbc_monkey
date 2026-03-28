# odbc-monkey Native Driver

This directory contains the native C++ ODBC driver, CMake project, headers, and native test suite for `odbc-monkey`.

## What Is Here

- `src/` - ODBC exports, JSON data source, file watcher, logger
- `include/` - driver headers and vendored `nlohmann/json`
- `test/` - native ODBC test suite and test JSON data
- `build.ps1` - supported build entry point
- `setup_build_tools.ps1` - installs local build prerequisites
- `CMakeLists.txt` - CMake project definition

## Supported Build Flow

The supported way to build this project is:

```powershell
cd native
.\build.ps1
```

Do not rely on old `build.ps1 -CMake` or `-Clang` flags from older docs. The current script always builds through CMake using Visual Studio 2022.

## Prerequisites

Required:

- Windows x64
- Visual Studio 2022 Build Tools with C++ workload
- CMake

Recommended:

- `clang-format` for source formatting during builds when available

To install the default toolchain:

```powershell
cd native
.\setup_build_tools.ps1
```

This installs Visual Studio Build Tools and `clang-format` by default. After setup, open a new terminal so updated `PATH` entries are visible.

## Build Commands

Build release:

```powershell
.\build.ps1
```

Build debug:

```powershell
.\build.ps1 -Debug
```

Clean and rebuild:

```powershell
.\build.ps1 -Clean
```

Build and run the native test workflow:

```powershell
.\build.ps1 -Test
```

This auto-installs the current build before running the native suite through the Windows Driver Manager.

## Build Outputs

Important outputs:

- `cmake-build\Release\odbc-monkey_<version>.dll`
- `cmake-build\Release\odbc_test.exe`
- `build\odbc-monkey.dll`
- `..\bin\odbc-monkey_<version>.dll`

The top-level `bin/` copy is the packaged driver artifact for the repo.

## Native Tests

The native test executable exercises the ODBC behavior used by Altium, including:

- connection and handle management
- statement attributes
- table and column discovery
- query execution and fetch behavior
- classification table access
- Unicode handling
- file watcher cache refresh

The tests live in `test\odbc_test.cpp` and use JSON fixtures under `test\test_data\`.

`.\build.ps1 -Test` is the easiest way to run them through the supported project workflow.

## Manual CMake Use

If you want to work directly with CMake instead of the PowerShell wrapper:

```powershell
cmake -G "Visual Studio 17 2022" -A x64 -B cmake-build
cmake --build cmake-build --config Release --parallel
```

This is supported for development, but `build.ps1` is the canonical project workflow.

## Troubleshooting

If `build.ps1` says Visual Studio 2022 was not found:

- run `.\setup_build_tools.ps1`
- reopen the terminal

If the DLL builds but applications cannot connect:

- confirm the connection string includes `DataSource=...`
- confirm the JSON directory contains files using the expected `versions` format
