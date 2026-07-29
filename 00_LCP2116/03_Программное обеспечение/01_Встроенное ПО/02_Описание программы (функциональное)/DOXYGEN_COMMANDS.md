# Генерация Doxygen для LCP Basic 1.02.0

## 1. Подготовка рабочей папки

Рабочая папка:

```bat
C:\DoxygenFolder
```

В неё необходимо скопировать **содержимое** каталога:

```text
TestRepo\00_LCP2116\RTOS\
```

То есть `main.cpp`, `Config`, `app`, `board`, `hal`, `libs`, `middleware`,
`platform`, `protocol`, `tools` и остальные файлы должны находиться прямо в
`C:\DoxygenFolder`, без дополнительного уровня `RTOS`.

Затем из каталога:

```text
TestRepo\00_LCP2116\02_Описание программы (функциональное)\
```

скопировать в тот же корень четыре файла:

```text
README.md
Doxyfile
build_doxygen.bat
DOXYGEN_COMMANDS.md
```

Ожидаемая структура:

```text
C:\DoxygenFolder\
├── Config\
├── app\
├── board\
├── hal\
├── libs\
├── middleware\
├── platform\
├── protocol\
├── tools\
├── README.md
├── DOXYGEN_COMMANDS.md
├── Doxyfile
├── build_doxygen.bat
└── main.cpp
```

`Device_Startup`, `middleware\FreeRTOS-Kernel` и `libs\SAM_USB` могут находиться
в рабочей копии, но исключены из публичной Doxygen-документации как startup и
сторонний/platform support код.

## 2. Проверка Doxygen

Открыть `cmd.exe` и выполнить:

```bat
cd /d C:\DoxygenFolder
where doxygen
doxygen --version
```

Если `doxygen.exe` установлен, но не найден, временно добавить типовой путь:

```bat
set "PATH=C:\Program Files\doxygen\bin;%PATH%"
where doxygen
doxygen --version
```

## 3. Рекомендуемый запуск

```bat
cd /d C:\DoxygenFolder
build_doxygen.bat
```

Скрипт:

- проверяет полноту структуры LCP Basic 1.02.0;
- проверяет доступность `doxygen.exe`;
- удаляет старую папку результата;
- запускает Doxygen с логом;
- проверяет наличие `index.html`;
- открывает созданное руководство в браузере.

Результат:

```text
C:\DoxygenFolder\docs\doxygen\html\index.html
```

Логи:

```text
C:\DoxygenFolder\docs\doxygen\doxygen_build.log
C:\DoxygenFolder\docs\doxygen\doxygen_warnings.log
```

## 4. Ручной запуск без bat-файла

```bat
cd /d C:\DoxygenFolder
if exist docs\doxygen rmdir /s /q docs\doxygen
mkdir docs\doxygen
doxygen Doxyfile > docs\doxygen\doxygen_build.log 2>&1
```

После успешного завершения:

```bat
start "" "docs\doxygen\html\index.html"
```

## 5. Проверка результата

В созданном HTML должны отображаться:

- версия проекта `1.02.0`;
- главная страница LCP Basic;
- каталоги `app`, `board`, `hal`, `platform`, `protocol`;
- библиотеки `lcp_crc32` и `lcp_sd_storage`;
- исходный код через раздел «Файлы»;
- структуры и публичные функции подсистем конфигурации, X2X, FieldSensor,
  Modbus RTU/TCP, Ethernet, RTC, watchdog и service console.

Каталоги `middleware\FreeRTOS-Kernel`, `libs\SAM_USB` и `Device_Startup` в
публичном дереве файлов отображаться не должны.
