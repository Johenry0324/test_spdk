@echo off
REM Build script for Windows (will use WSL if available)

echo ==========================================
echo Building SPDK and sequential_write tool
echo ==========================================
echo.

REM Check if WSL is available
where wsl >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: WSL is not available or not in PATH
    echo Please install WSL or run the build script from WSL directly
    exit /b 1
)

echo Using WSL to build...
echo.

REM Get the current directory and convert to WSL path
set "CURRENT_DIR=%~dp0"
set "CURRENT_DIR=%CURRENT_DIR:~0,-1%"

REM Convert Windows path to WSL path
for /f "tokens=*" %%i in ('wsl wslpath "%CURRENT_DIR%\.."') do set WSL_PATH=%%i

REM Run the build script in WSL
wsl bash "%WSL_PATH%/scripts/build_spdk.sh"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ==========================================
    echo Build completed successfully!
    echo ==========================================
) else (
    echo.
    echo ==========================================
    echo Build failed!
    echo ==========================================
    exit /b 1
)

