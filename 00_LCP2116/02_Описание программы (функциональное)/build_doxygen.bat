@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo LCP Basic Diagnostic Firmware - Doxygen build
echo Working directory: %CD%

if not exist "README.md" (
    echo ERROR: README.md not found in project root.
    exit /b 1
)

if not exist "Doxyfile" (
    echo ERROR: Doxyfile not found in project root.
    exit /b 1
)

where doxygen >nul 2>nul
if errorlevel 1 (
    echo ERROR: doxygen.exe not found in PATH.
    echo Install Doxygen or add Doxygen bin folder to PATH.
    exit /b 1
)

if not exist "docs" mkdir "docs"
if not exist "docs\doxygen" mkdir "docs\doxygen"

echo Running Doxygen...
doxygen Doxyfile > "docs\doxygen\doxygen_build.log" 2>&1

if errorlevel 1 (
    echo ERROR: Doxygen finished with errors.
    echo See docs\doxygen\doxygen_build.log
    exit /b 1
)

if exist "docs\doxygen\html\index.html" (
    echo OK: docs\doxygen\html\index.html
    start "" "docs\doxygen\html\index.html"
) else (
    echo ERROR: index.html was not generated.
    echo See docs\doxygen\doxygen_build.log
    exit /b 1
)

exit /b 0
