@echo off
setlocal enabledelayedexpansion

echo === libmctp-mmbi Build and Test Automation ===

:: Set build directory
set BUILD_DIR=build

:: Clean build directory if requested
if "%1"=="clean" (
    echo Cleaning build directory...
    if exist %BUILD_DIR% rd /s /q %BUILD_DIR%
)

:: Create build directory
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

:: Configure
echo Configuring with CMake...
cmake -S . -B %BUILD_DIR%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b %ERRORLEVEL%
)

:: Build
echo Building...
cmake --build %BUILD_DIR% --config Debug
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    exit /b %ERRORLEVEL%
)

:: Run Tests
echo Running Tests...
cd %BUILD_DIR%\Debug
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Could not enter build\Debug directory.
    exit /b 1
)

set TEST_FAILED=0

echo [1/14] Running test_eid.exe...
.\test_eid.exe
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

echo [2/14] Running test_seq.exe...
.\test_seq.exe
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

echo [3/14] Running test_bridge.exe...
.\test_bridge.exe
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

echo [4/14] Running test_cmds.exe...
.\test_cmds.exe
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

echo [5/14] Running test_core.exe...
.\test_core.exe
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

echo [6/14] Running test_mmbi.exe...
.\test_mmbi.exe
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

echo [7/14] Running test_mmbi_host.exe...
:: Need to provide args? Usually just runs
.\test_mmbi_host.exe
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

echo [8/14] Running test_mmbi_bmc.exe...
.\test_mmbi_bmc.exe
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

echo [9/14] Running test_file_transfer.exe...
:: Requires args, so skip or run with --help
.\test_file_transfer.exe --help
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

echo [10/14] Running test_stress_mmbi.exe...
.\test_stress_mmbi.exe
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

echo [11/14] Running test_mmbi_log.exe...
.\test_mmbi_log.exe
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

echo [12/14] Running host_transport.exe...
.\host_transport.exe --help
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

echo [13/14] Running bmc_transport.exe...
.\bmc_transport.exe --help
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

echo [14/14] Running test_mmbi_full_duplex.exe...
.\test_mmbi_full_duplex.exe
if %ERRORLEVEL% neq 0 set TEST_FAILED=1

if %TEST_FAILED% neq 0 (
    echo [ERROR] One or more tests failed.
    exit /b 1
)

echo === Build and Test Successful ===
exit /b 0
