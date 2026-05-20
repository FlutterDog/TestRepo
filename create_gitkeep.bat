@echo off
REM Скрипт для добавления .gitkeep во все пустые папки

setlocal enabledelayedexpansion

set root=.

for /f "delims=" %%d in ('dir /ad /b /s "%root%"') do (
    dir /a-d "%%d" | findstr /r /c:"^[ ]*[0-9][0-9]*[ ]" >nul
    if errorlevel 1 (
        echo Добавляю .gitkeep в %%d
        echo.>"%%d\.gitkeep"
    )
)

echo Готово!
pause
