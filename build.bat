@echo off
REM ============================================================================
REM WASAPI Build Script (Windows)
REM ============================================================================

echo ========================================
echo Building WASAPI library
echo ========================================

REM Force English compiler output for clean logs
set VSLANG=1033

REM Create build and logs directories
if not exist build mkdir build
if not exist logs mkdir logs

REM Timestamp for log file (PowerShell, works on Win10/11)
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd_HH-mm-ss"') do set TS=%%i
set LOG=logs\%TS%_build.log

cd build

REM Configure with CMake
echo.
echo [1/4] Configuring CMake...
cmake .. -G "Visual Studio 17 2022" -A x64 -DWASAPI_BUILD_EXAMPLES=ON -DCMAKE_INSTALL_PREFIX="../install" -DCMAKE_VS_GLOBALS="PreferredUILang=en-US"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed!
    cd ..
    exit /b 1
)

REM Build
echo.
echo [2/4] Building...
cmake --build . --config Release -- /p:PreferredUILang=en-US /nologo > "..\%LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed! Check %LOG%
    cd ..
    exit /b 1
)

REM Install
echo.
echo [3/4] Installing to install/ folder...
cmake --install . --config Release >> "..\%LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Install failed! Check %LOG%
    cd ..
    exit /b 1
)

echo.
echo [4/4] Done! WASAPI build and install successful.
echo Log: %LOG%
echo ========================================
echo Output: install\lib\wasapi.lib
echo         install\include\wasapi\
echo ========================================
cd ..
exit /b 0
