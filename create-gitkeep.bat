@echo off
setlocal

cd /d "%~dp0"

for /d /r %%D in (*) do (
    if /i not "%%~nxD"==".git" (
        if not exist "%%D\.gitkeep" (
            type nul > "%%D\.gitkeep"
            echo created: %%D\.gitkeep
        )
    )
)

echo done
endlocal
pause