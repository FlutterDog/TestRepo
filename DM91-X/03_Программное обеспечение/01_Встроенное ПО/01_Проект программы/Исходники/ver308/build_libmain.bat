@echo off
setlocal enabledelayedexpansion

rem ---- paths ----
set "SCRIPT_DIR=%~dp0"
rem локальный тулчейн рядом с проектом:
set "AVR_BIN=%SCRIPT_DIR%DM91_Make\toolchain\bin"
if not exist "%AVR_BIN%\avr-gcc.exe" (
  echo [ERR] avr-gcc.exe not found at: "%AVR_BIN%\avr-gcc.exe"
  echo       Put portable toolchain into .\DM91_Make\toolchain\bin\
  pause
  exit /b 1
)
set "PATH=%AVR_BIN%;%PATH%"

rem ---- flags ----
set "MCU=atmega328p"
set "CFLAGS=-mmcu=%MCU% -Os -Wall -ffunction-sections -fdata-sections -fpack-struct -fshort-enums -mrelax"
set "CPPFLAGS=%CFLAGS% -fno-exceptions -fno-rtti"
set "INCLUDES=-Isrc -Isrc\LibFiles -Isrc\LibFiles\cores -Isrc\LibFiles\cores\arduino -Isrc\LibFiles\cores\arduino\avr-libc -Isrc\LibFiles\variants -Isrc\LibFiles\variants\standard -Isrc\LibFiles\libraries -Isrc\ProjectFiles -Isrc\LibFiles\cores\arduino\utility -I%SCRIPT_DIR%DM91_Make\toolchain\avr\include"

cd /d "%SCRIPT_DIR%"

echo [1] clean objects
del /Q *.o *.opp >nul 2>&1

echo [2] compile .c
avr-gcc %CFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\avr-libc\malloc.c"         -o malloc.o        || goto :error
avr-gcc %CFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\avr-libc\realloc.c"        -o realloc.o       || goto :error
avr-gcc %CFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\utility\twi.c"             -o twi.o           || goto :error
avr-gcc %CFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\WInterrupts.c"             -o WInterrupts.o   || goto :error
avr-gcc %CFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\wiring.c"                  -o wiring.o        || goto :error
avr-gcc %CFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\wiring_analog.c"           -o wiring_analog.o || goto :error
avr-gcc %CFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\wiring_digital.c"          -o wiring_digital.o|| goto :error
avr-gcc %CFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\wiring_pulse.c"            -o wiring_pulse.o  || goto :error
avr-gcc %CFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\wiring_shift.c"            -o wiring_shift.o  || goto :error
avr-gcc %CFLAGS% %INCLUDES% -c "src\LibFiles\libraries\sht31_i2c.c"                   -o sht31_i2c.o     || goto :error

echo [3] compile .cpp
avr-g++ %CPPFLAGS% %INCLUDES% -c "Main.cpp"                                           -o Main.o          || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\CDC.cpp"                 -o CDC.o           || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\HardwareSerial.cpp"      -o HardwareSerial.o|| goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\HID.cpp"                 -o HID.o           || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\IPAddress.cpp"           -o IPAddress.o     || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\main.cpp"                -o main_core.o     || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\new.cpp"                 -o new.o           || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\Print.cpp"               -o Print.o         || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\Stream.cpp"              -o Stream.o        || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\Tone.cpp"                -o Tone.o          || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\USBCore.cpp"             -o USBCore.o       || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\Wire.cpp"                -o Wire.o          || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\WMath.cpp"               -o WMath.o         || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\cores\arduino\WString.cpp"             -o WString.o       || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\libraries\AltSoftSerial.cpp"           -o AltSoftSerial.o || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\libraries\DS18B20.cpp"                 -o DS18B20.o       || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\libraries\I2C.cpp"                     -o I2C.o           || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\libraries\IGAS_485DN.cpp"              -o IGAS_485DN.o    || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\libraries\IGAS_display.cpp"            -o IGAS_display.o  || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\libraries\IGAS_Eeprom.cpp"             -o IGAS_Eeprom.o   || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\libraries\OneWire.cpp"                 -o OneWire.o       || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\libraries\SHT31.cpp"                   -o SHT31.o         || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\libraries\SPI.cpp"                     -o SPI.o           || goto :error
avr-g++ %CPPFLAGS% %INCLUDES% -c "src\LibFiles\libraries\TimerOne.cpp"                -o TimerOne.o      || goto :error

echo [4] archive
set "OUTLIB=libDM-910-x.a"
del /q "%OUTLIB%" >nul 2>&1
avr-ar rcs "%OUTLIB%" *.o || goto :error
avr-ar t "%OUTLIB%"

echo [5] cleanup
del /q *.o >nul 2>&1

echo [OK] %OUTLIB% built
pause
exit /b 0

:error
echo [!] COMPILE ERROR
pause
exit /b 1
