@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"

echo ============================================================
echo LCP Basic Diagnostic Firmware 1.02.0 - Doxygen build
echo Working directory: %CD%
echo ============================================================

for %%F in (README.md DOXYGEN_COMMANDS.md Doxyfile main.cpp) do (
    if not exist "%%F" (
        echo ERROR: required file %%F not found in project root.
        echo Copy the current RTOS sources and Doxygen support files into one folder.
        exit /b 1
    )
)

for %%D in (Config app board hal platform protocol libs) do (
    if not exist "%%D\" (
        echo ERROR: required directory %%D not found in project root.
        echo The prepared folder does not contain the complete LCP Basic 1.02.0 source tree.
        exit /b 1
    )
)

if not exist "libs\lcp_crc32\" (
    echo ERROR: libs\lcp_crc32 not found.
    exit /b 1
)

if not exist "libs\lcp_sd_storage\" (
    echo ERROR: libs\lcp_sd_storage not found.
    exit /b 1
)

where doxygen >nul 2>nul
if errorlevel 1 (
    echo ERROR: doxygen.exe not found in PATH.
    echo Install Doxygen and add its bin directory to PATH.
    echo Typical location: C:\Program Files\doxygen\bin
    exit /b 1
)

for /f "delims=" %%V in ('doxygen --version') do set "DOXYGEN_VERSION=%%V"
echo Doxygen version: %DOXYGEN_VERSION%

if exist "docs\doxygen" rmdir /s /q "docs\doxygen"
mkdir "docs\doxygen"

set "BUILD_LOG=docs\doxygen\doxygen_build.log"
set "WARNING_LOG=docs\doxygen\doxygen_warnings.log"

echo Running Doxygen...
doxygen Doxyfile > "%BUILD_LOG%" 2>&1
set "DOXYGEN_EXIT=%ERRORLEVEL%"

if not "%DOXYGEN_EXIT%"=="0" (
    echo ERROR: Doxygen finished with exit code %DOXYGEN_EXIT%.
    echo See %BUILD_LOG%
    exit /b %DOXYGEN_EXIT%
)

if not exist "docs\doxygen\html\index.html" (
    echo ERROR: index.html was not generated.
    echo See %BUILD_LOG%
    exit /b 1
)

if exist "%WARNING_LOG%" (
    for %%A in ("%WARNING_LOG%") do if %%~zA GTR 0 (
        echo WARNING: Doxygen generated warnings.
        echo See %WARNING_LOG%
    )
)

echo OK: docs\doxygen\html\index.html
echo Build log: %BUILD_LOG%
start "" "docs\doxygen\html\index.html"

exit /b 0
