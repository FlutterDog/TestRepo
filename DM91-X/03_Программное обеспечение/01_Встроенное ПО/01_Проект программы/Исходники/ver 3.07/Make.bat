@echo off
setlocal enabledelayedexpansion

:: Установить путь к инструментам AVR GCC (замени, если у тебя другой путь)
set AVR_BIN=C:\Program Files (x86)\Atmel\Studio\7.0\toolchain\avr8\avr8-gnu-toolchain\bin
set PATH=%AVR_BIN%;%PATH%

set MCU=atmega328p
set CFLAGS=-mmcu=%MCU% -Os -Wall -ffunction-sections -fdata-sections -fpack-struct -fshort-enums -mrelax
set CPPFLAGS=%CFLAGS% -fno-exceptions -fno-rtti
set INCLUDES=-I../src -I../src/LibFiles -I../src/LibFiles/cores -I../src/LibFiles/cores/arduino ^
-I../src/LibFiles/cores/arduino/avr-libc -I../src/LibFiles/variants -I../src/LibFiles/variants/standard ^
-I../src/LibFiles/libraries -I../src/ProjectFiles -I../src/LibFiles/cores/arduino/utility ^
-I"C:\Program Files (x86)\Atmel\Studio\7.0\Packs\Atmel\ATmega_DFP\1.7.374\include"

echo [1] Очистка .o
del /Q *.o >nul 2>&1

echo [2] Компиляция исходников...

:: Скомпилировать вручную все нужные файлы

rem C-файлы
for %%F in (
  "../src/LibFiles/cores/arduino/avr-libc/malloc.c"
  "../src/LibFiles/cores/arduino/avr-libc/realloc.c"
  "../src/LibFiles/cores/arduino/utility/twi.c"
  "../src/LibFiles/cores/arduino/WInterrupts.c"
  "../src/LibFiles/cores/arduino/wiring.c"
  "../src/LibFiles/cores/arduino/wiring_analog.c"
  "../src/LibFiles/cores/arduino/wiring_digital.c"
  "../src/LibFiles/cores/arduino/wiring_pulse.c"
  "../src/LibFiles/cores/arduino/wiring_shift.c"
  "../src/LibFiles/libraries/sht31_i2c.c"
) do (
  echo Compiling %%F
  avr-gcc %CFLAGS% %INCLUDES% -c %%F -o %%~nF.o || goto :error
)

rem C++-файлы
for %%F in (
  "../Main.cpp"
  "../src/LibFiles/cores/arduino/CDC.cpp"
  "../src/LibFiles/cores/arduino/HardwareSerial.cpp"
  "../src/LibFiles/cores/arduino/HID.cpp"
  "../src/LibFiles/cores/arduino/IPAddress.cpp"
  "../src/LibFiles/cores/arduino/main.cpp"
  "../src/LibFiles/cores/arduino/new.cpp"
  "../src/LibFiles/cores/arduino/Print.cpp"
  "../src/LibFiles/cores/arduino/Stream.cpp"
  "../src/LibFiles/cores/arduino/Tone.cpp"
  "../src/LibFiles/cores/arduino/USBCore.cpp"
  "../src/LibFiles/cores/arduino/Wire.cpp"
  "../src/LibFiles/cores/arduino/WMath.cpp"
  "../src/LibFiles/cores/arduino/WString.cpp"
  "../src/LibFiles/libraries/AltSoftSerial.cpp"
  "../src/LibFiles/libraries/DS18B20.cpp"
  "../src/LibFiles/libraries/I2C.cpp"
  "../src/LibFiles/libraries/IGAS_485DN.cpp"
  "../src/LibFiles/libraries/IGAS_display.cpp"
  "../src/LibFiles/libraries/IGAS_Eeprom.cpp"
  "../src/LibFiles/libraries/OneWire.cpp"
  "../src/LibFiles/libraries/SHT31.cpp"
  "../src/LibFiles/libraries/SPI.cpp"
  "../src/LibFiles/libraries/TimerOne.cpp"
) do (
  echo Compiling %%F
  avr-g++ %CPPFLAGS% %INCLUDES% -c %%F -o %%~nF.o || goto :error
)

echo [3] Создание libmain.a
avr-ar rcs libmain.a *.o

echo [✔] Библиотека готова: libmain.a
pause
exit /b 0

:error
echo [!] Ошибка компиляции
pause
exit /b 1
