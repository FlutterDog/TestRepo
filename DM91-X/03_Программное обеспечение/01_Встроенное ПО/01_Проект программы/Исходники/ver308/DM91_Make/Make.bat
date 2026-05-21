@echo off
REM Определяем путь к нашей копии тулчейна рядом со скриптом
set "SCRIPT_DIR=%~dp0"
set "AVR_BIN=%SCRIPT_DIR%toolchain\bin"

set MCU=atmega328p

REM Общие флаги для компиляции С/С++
set CFLAGS=-Os -mrelax -ffunction-sections -fdata-sections -mmcu=%MCU%
REM Общие флаги для линковки
set LDFLAGS=-Wl,--gc-sections -Wl,--relax -mmcu=%MCU% -Wl,--section-start=.config=0x7700

echo [1] Компиляция config.c…
"%AVR_BIN%\avr-gcc" %CFLAGS% -c config.c -o config.o || goto :error

echo [2] Линковка прошивки…
"%AVR_BIN%\avr-g++" config.o libDM-910-x.a %LDFLAGS% -o firmware.elf || goto :error

echo [3] Генерация Intel HEX…
"%AVR_BIN%\avr-objcopy" -O ihex -R .eeprom firmware.elf firmware.hex || goto :error

echo.
echo *** Сборка успешно завершена! Размер: ***
for %%F in (firmware.hex) do @echo %%~zF bytes

echo.
echo [4] Удаление промежуточных файлов…
del config.o         2>nul
del firmware.elf     2>nul

goto :eof

:error
echo.
echo !!! Ошибка сборки. Проверьте вывод выше.
pause
