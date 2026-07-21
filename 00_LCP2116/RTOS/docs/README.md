<!--
Титульный блок подготовлен для будущих логотипов и иконок.
Рекомендуемая структура файлов:

00_LCP2116/RTOS/docs/assets/lorentz-logo.svg
00_LCP2116/RTOS/docs/assets/lcp2116-controller.png

После добавления файлов можно раскомментировать пример ниже.

<p align="center">
  <img src="assets/lorentz-logo.svg" alt="Lorentz" width="240">
</p>
-->

<div align="center">

# Lorentz LCP2116

## LCP Basic Firmware

**Руководство пользователя и разработчика**

Версия прошивки: **1.00.0**  
Целевая платформа: **ATSAM3X8E / ARM Cortex-M3**

</div>

---

<a id="contents"></a>

## Оглавление

1. [Назначение прошивки](#purpose)
2. [Быстрый старт](#quick-start)
3. [Архитектура проекта](#architecture)
4. [Сборка и прошивка](#build)
5. [Подготовка microSD](#microsd)
6. [USB service console](#console)
7. [Команды и диагностика](#commands)
8. [FieldSensor и порты S1–S4](#fieldsensor)
9. [Подключение другого Modbus RTU-прибора](#new-field-device)
10. [Ethernet и Modbus TCP](#ethernet)
11. [Шина X2X](#x2x)
12. [Добавление нового X2X-модуля](#new-x2x-module)
13. [Разработка неблокирующего X2X-драйвера](#x2x-driver)
14. [Добавление console-команды](#new-console-command)
15. [Типовые неисправности](#troubleshooting)
16. [Правила разработки](#development-rules)
17. [Проверка перед выпуском](#release-checklist)
18. [Карта документации](#documentation-map)

> В интерфейсе GitHub также доступно автоматическое меню **Outline**, построенное по заголовкам этого файла.

---

<a id="purpose"></a>

## 1. Назначение прошивки

`LCP Basic Diagnostic Firmware` — базовая прошивка контроллера LCP2116. Она используется:

- для проверки аппаратной части контроллера;
- для работы с внутренней модульной шиной X2X;
- для опроса внешних Modbus RTU-приборов через S1–S4;
- для публикации данных через два Ethernet-интерфейса;
- для диагностики microSD, RTC, watchdog, UART, памяти и сетевых интерфейсов;
- как основа для разработки прикладного проекта.

В baseline входят:

- FreeRTOS;
- одна основная прикладная задача LCP;
- X2X master на UART0;
- четыре независимых Modbus RTU master на S1–S4;
- два W5500 и два Modbus TCP server;
- USB CDC service console;
- microSD/FAT;
- RTC;
- hardware watchdog;
- сервисные и диагностические команды.

В baseline намеренно не входят:

- PNX Studio;
- прикладная PLC-логика конкретной установки;
- IEC 60870-5-104;
- логика операторской панели;
- универсальная база внешних приборов;
- автоматическое применение `EIA.TXT`.

Baseline является рабочей основой. Новые приборы, протоколы и алгоритмы добавляются поверх существующих слоёв `app`, `board`, `hal` и `protocol`.

[↑ К оглавлению](#contents)

---

<a id="quick-start"></a>

## 2. Быстрый старт

Минимальная последовательность запуска:

1. Открыть проект:

   ```text
   00_LCP2116/RTOS/LCP_Basic.cppproj
   ```

2. Собрать конфигурацию `Release`.
3. Скопировать конфигурационные файлы из `sd_card/` в корень microSD.
4. Прошить контроллер.
5. Подключить USB service console.
6. Выполнить:

   ```text
   version
   help
   status
   rtos
   sd
   field
   x2x
   eth
   time
   watchdog
   ```

Ожидаемая версия:

```text
version = 1.00.0
stage = Release 1.00.0
target = ATSAM3X8E
```

Для быстрой проверки интерфейсов:

```text
field   -> состояния S1–S4
x2x     -> внутренняя модульная шина
eth     -> ETH1, ETH2 и Modbus TCP map
rtos    -> SRAM, heap и task stack
```

[↑ К оглавлению](#contents)

---

<a id="architecture"></a>

## 3. Архитектура проекта

### 3.1 Поток выполнения

```text
main.cpp
  -> app_rtos_start()
      -> одна FreeRTOS task LCP
          -> setup
          -> постоянный loop
              -> microSD service
              -> X2X service
              -> FieldSensor S1..S4
              -> Ethernet ETH1/ETH2
              -> RTC
              -> watchdog
              -> USB console
```

Все прикладные service работают неблокирующе внутри одной задачи. Если одна функция долго ждёт аппаратное событие, она задерживает остальные подсистемы.

### 3.2 Каталоги

```text
app/
    прикладные service, runtime-состояния и диагностика

board/
    привязка логических интерфейсов к плате LCP2116

hal/
    ATSAM3X8E, SC16IS7xx, W5500 и низкоуровневая периферия

protocol/
    универсальные Modbus RTU и Modbus TCP engine

platform/
    Serial, SPI, GPIO, millis и platform-функции

libs/
    отдельные reusable-библиотеки

sd_card/
    штатные конфигурационные файлы microSD

docs/
    эксплуатационная и разработческая документация
```

### 3.3 Владение интерфейсами

Один физический интерфейс должен иметь одного владельца.

```text
UART0       -> X2X master
               либо fallback echo при пустой X2X-конфигурации

S1..S4      -> FieldSensor Modbus RTU master

ETH1        -> отдельный Modbus TCP server
ETH2        -> отдельный Modbus TCP server

PC/HMI UART -> diagnostic echo до появления прикладного протокола
```

Нельзя одновременно запускать два protocol engine на одном half-duplex UART.

### 3.4 Разделение уровней

```text
HAL       -> регистры MCU и физические операции
board     -> разводка и ресурсы конкретной платы
protocol  -> универсальная реализация протокола
service   -> применение протокола для конкретной задачи
status    -> отображение состояния и сервисные команды
```

Примеры неправильного смешения:

- W5500 HAL знает структуру FieldSensor;
- Modbus RTU master знает модель прибора;
- X2X-драйвер читает microSD самостоятельно;
- console напрямую меняет внутреннюю state machine protocol engine.

[↑ К оглавлению](#contents)

---

<a id="build"></a>

## 4. Сборка и прошивка

Проект Microchip Studio:

```text
00_LCP2116/RTOS/LCP_Basic.cppproj
```

Целевая платформа:

```text
ATSAM3X8E
ARM Cortex-M3
```

Рекомендуемый порядок:

1. собрать `Debug`;
2. проверить ошибки и предупреждения;
3. собрать `Release`;
4. проверить Flash и SRAM;
5. прошить Release-бинарник;
6. выполнить минимальный console-test.

В Build Output проверить:

```text
Build succeeded
0 failed
```

Также зафиксировать:

```text
warnings
text / Flash
.data
.bss / SRAM
```

Стандартное сообщение Atmel Studio о приблизительной оценке memory usage не является предупреждением исходного кода.

После добавления нового `.cpp` его необходимо включить в `LCP_Basic.cppproj` через Microchip Studio либо вручную:

```xml
<Compile Include="app\x2x\modules\x2x_example.cpp">
    <SubType>compile</SubType>
</Compile>
```

Заголовки подключаются через `#include`, но их также рекомендуется показывать в дереве проекта для навигации.

[↑ К оглавлению](#contents)

---

<a id="microsd"></a>

## 5. Подготовка microSD

Поддерживается FAT16/FAT32. Примеры находятся в:

```text
00_LCP2116/RTOS/sd_card/
```

Файлы копируются в корень карты.

### 5.1 FieldSensor

```text
baud.TXT
Parity.TXT
```

### 5.2 Ethernet

ETH1:

```text
MAC.txt
IP.txt
SUBNET.txt
GATE.txt
```

ETH2:

```text
MAC2.txt
IP2.txt
SUBNET2.txt
GATE2.txt
```

### 5.3 X2X

```text
X2X.TXT
```

### 5.4 Зарезервированный файл

```text
EIA.TXT
```

Файл сохранён для совместимости с прикладными проектами, но baseline runtime его не применяет.

### 5.5 Правила парсера

Поддерживаются:

- пустые строки;
- комментарии после `//`;
- комментарии после `#`;
- завершающая строка `fin` или `Fin` после требуемого числа значений.

Имена файлов являются частью внешнего формата. Регистр символов важен.

Правильно:

```text
baud.TXT
Parity.TXT
IP.txt
```

Неправильно:

```text
BAUD.TXT
PARITY.TXT
IP.TXT
```

Псевдонимы намеренно не поддерживаются.

[↑ К оглавлению](#contents)

---

<a id="console"></a>

## 6. USB service console

Прошивка создаёт USB CDC-порт. Можно использовать:

- PuTTY;
- Tera Term;
- RealTerm;
- другой serial terminal.

Рекомендуемые настройки:

```text
115200
8 data bits
no parity
1 stop bit
no flow control
```

Для USB CDC выбранный baudrate не определяет физическую скорость, но 115200 используется как стандартная настройка терминала.

После подключения:

```text
LCP firmware ready. Type 'help' or '?'.
```

Команды не чувствительны к регистру. Выполнение — клавишей Enter.

### 6.1 Формат вывода

Именованные параметры:

```text
name = value
name = value, next = value
```

Индексированные массивы:

```text
0: value, 1: value
```

Двоеточия внутри значения сохраняются:

```text
mac = DE:AD:BE:EF:FE:EE
datetime = 2026-07-20 21:30:00
```

Общие helpers находятся в:

```text
app/diagnostics/diagnostic_output.hpp
```

### 6.2 Навигация по справке

Начать следует с:

```text
help
```

Короткий alias:

```text
?
```

`help` показывает сгруппированный список команд. Для большого общего снимка используется `status`, но для ежедневной работы удобнее вызывать профильные команды.

[↑ К оглавлению](#contents)

---

<a id="commands"></a>

## 7. Команды и диагностика

### 7.1 Основные команды

| Команда | Назначение |
|---|---|
| `help`, `?` | Список команд по группам |
| `version`, `ver` | Версия, stage и target |
| `status` | Полный отчёт всех подсистем |
| `uptime` | Время работы в миллисекундах и привычном формате |
| `rtos` | SRAM, heap и stack в байтах и процентах |
| `periodic on` | Полный `status` каждые 10 секунд |
| `periodic off` | Отключение периодического отчёта |

### 7.2 microSD

```text
sd
sd test
```

`sd` показывает:

- состояние card detect;
- готовность filesystem;
- тип FAT;
- результат инициализации.

`sd test` создаёт и читает `SDTEST.TXT`. Команда изменяет содержимое карты и предназначена только для сервисной проверки записи.

### 7.3 RTC

Короткий вывод:

```text
time
rtc time
```

Полный отчёт:

```text
rtc
```

Установка времени:

```text
rtc set 2026-07-20 21:30:00
```

Операция неблокирующая. После команды другие service продолжают работу.

### 7.4 Watchdog

```text
watchdog
watchdog feed
watchdog test reset
```

`watchdog test reset` намеренно прекращает автоматический feed. Контроллер должен перезагрузиться.

### 7.5 UART и SC16IS

```text
rs485
sc16is
```

`rs485` показывает владельца, serial format и hardware errors каждого порта.

`sc16is` показывает:

- обнаруженную аппаратную конфигурацию внешних UART;
- наличие каналов A/B;
- назначение PC, HMI и S1.

`SC16.txt` не используется: внешние UART определяются автоматически.

### 7.6 Признаки нормальной работы RTOS

Выполнить:

```text
rtos
```

Нормально:

- current free heap не уменьшается после одинаковых действий;
- minimum-ever-free после начального прогона стабилизируется;
- task stack имеет достаточный резерв;
- uptime не сбрасывается без причины.

### 7.7 Полный диагностический снимок

```text
status
```

Отчёт содержит:

- SYSTEM;
- RTOS SUMMARY;
- RS-485 PORTS;
- X2X;
- FieldSensor;
- SC16IS;
- Ethernet;
- microSD;
- battery;
- RTC;
- watchdog.

[↑ К оглавлению](#contents)

---

<a id="fieldsensor"></a>

## 8. FieldSensor и порты S1–S4

FieldSensor — демонстрационный Modbus RTU master внешнего прибора.

Все четыре порта по умолчанию опрашивают:

```text
slave address = 1
function = FC03 Read Holding Registers
start register = 0
register count = 2
poll period = 300 ms
timeout = 500 ms
```

Команды:

```text
field
field reload
field pause
field resume
```

### 8.1 Физическое соответствие

```text
S1 -> SC16IS7xx
S2 -> ATSAM3X8E UART1 / Serial1
S3 -> ATSAM3X8E UART3 / Serial3
S4 -> ATSAM3X8E UART2 / Serial2
```

Для S1 автоматически определяется подходящий SC16IS-канал.

### 8.2 baud.TXT

```text
4
9600
9600
1200
9600
fin
```

Значения идут в порядке:

```text
S1
S2
S3
S4
```

Это фактические baudrate, а не индексы таблицы.

### 8.3 Parity.TXT

```text
4
0
0
0
0
fin
```

Коды:

```text
0 = 8N1
1 = 8O1
2 = 8E1
```

Скорость и parity загружаются независимо. При ошибке одного файла другая группа параметров может быть применена успешно.

### 8.4 Диагностика порта

Для каждого S1–S4 команда `field` показывает:

- роль;
- наличие transport;
- фактический baudrate и frame;
- slave address;
- start/count;
- period/timeout;
- connection/valid/request;
- последние регистры;
- success/failed;
- consecutive failures;
- UART errors;
- время последнего успешного обновления.

Нормальный подключённый прибор:

```text
connection = online
valid = yes
last_result = ok
consecutive_failures = 0
uart_errors = 0
```

После пяти последовательных ошибок:

```text
connection = lost
valid = no
```

Последние корректные значения сохраняются. Использовать их разрешено только при `valid = yes`.

### 8.5 Reload и pause

```text
field reload
```

Повторно читает `baud.TXT` и `Parity.TXT`. Активные транзакции завершаются, после чего UART переинициализируются.

```text
field pause
field resume
```

Pause запрещает новые запросы, но не обрывает активный RTU-кадр.

### 8.6 Быстрая проверка

1. Подключить slave address 1.
2. Настроить скорость и parity соответствующего порта.
3. Выполнить `field`.
4. Проверить рост `success`.
5. Отключить линию.
6. Проверить `lost` и `valid = no`.
7. Восстановить линию.
8. Проверить автоматический переход в `online`.

[↑ К оглавлению](#contents)

---

<a id="new-field-device"></a>

## 9. Подключение другого Modbus RTU-прибора

### 9.1 Где задаётся прибор

Файл:

```text
app/field/field_sensor_service.cpp
```

Функция:

```cpp
set_default_configs()
```

Основные параметры:

```cpp
g_configs[index].slave_address
g_configs[index].start_register
g_configs[index].register_count
g_configs[index].poll_period_ms
g_configs[index].timeout_ms
```

Пример:

```cpp
g_configs[index].role = FIELD_PORT_MASTER;
g_configs[index].slave_address = 7U;
g_configs[index].start_register = 100U;
g_configs[index].register_count = 4U;
g_configs[index].poll_period_ms = 1000U;
g_configs[index].timeout_ms = 700U;
```

### 9.2 Размер буфера

Текущее значение:

```cpp
FIELD_SENSOR_REGISTER_COUNT = 2U
```

Файл:

```text
app/field/field_sensor_service.hpp
```

Если прибор возвращает больше регистров, одновременно проверить:

1. `FIELD_SENSOR_REGISTER_COUNT`;
2. `FieldSensorPortState::registers`;
3. `set_default_configs()`;
4. декодирование данных;
5. `field_status.cpp`;
6. `ethernet_modbus_service.*`;
7. `ethernet_status.cpp`;
8. Modbus TCP-документацию.

Нельзя увеличить только `register_count`, оставив прежний runtime buffer.

### 9.3 Когда нужен отдельный service

Изменение baseline достаточно для простого прибора с одним FC03-запросом.

Отдельный service рекомендуется, если прибор требует:

- нескольких последовательных запросов;
- FC10 или других операций записи;
- собственной state machine;
- особого декодирования;
- отдельных quality-флагов;
- собственной диагностики.

`ModbusRtuMaster` должен оставаться универсальным protocol engine и не знать модель прибора.

### 9.4 Неблокирующий шаблон

```text
1. вызвать modbus_rtu_master_poll()
2. проверить завершение активного запроса
3. обработать result
4. при наступлении срока запустить новый запрос
5. сразу вернуть управление task
```

Запрещено:

```text
delay(...)
while (ожидание ответа)
busy-wait аппаратного флага
ожидание без timeout
```

### 9.5 Альтернативный slave-режим

Reusable FC03 slave находится в:

```text
protocol/modbus_rtu/modbus_rtu_slave.cpp
protocol/modbus_rtu/modbus_rtu_slave.hpp
```

Отключённый пример:

```text
app/field/field_sensor_slave_example.cpp
```

Baseline оставляет S1–S4 в режиме master.

[↑ К оглавлению](#contents)

---

<a id="ethernet"></a>

## 10. Ethernet и Modbus TCP

Оба W5500 запускают независимые server instance.

```text
protocol = Modbus TCP
port = 502
function = FC03 Read Holding Registers
holding registers = 0..11
write functions = not supported
```

ETH1 и ETH2 публикуют одинаковую карту FieldSensor.

### 10.1 Конфигурационные файлы

ETH1:

```text
MAC.txt
IP.txt
SUBNET.txt
GATE.txt
```

ETH2:

```text
MAC2.txt
IP2.txt
SUBNET2.txt
GATE2.txt
```

IP-файл:

```text
4
192
168
1
1
fin
```

MAC-файл:

```text
6
100
200
249
154
182
220
fin
```

MAC задаётся десятичными байтами.

Одинаковый IP на ETH1 и ETH2 допустим только в физически раздельных сетях. В одной LAN адреса должны различаться.

### 10.2 Применение настроек

```text
eth reload
```

Команда повторно читает восемь файлов и переинициализирует оба W5500.

Проверка:

```text
eth
```

Ожидается:

```text
initialized = yes
init = ok
VERSIONR = 0x04
link = up
```

### 10.3 Holding register map

| Адрес | Значение |
|---:|---|
| 0 | S1 value0 |
| 1 | S1 value1 |
| 2 | S1 quality |
| 3 | S2 value0 |
| 4 | S2 value1 |
| 5 | S2 quality |
| 6 | S3 value0 |
| 7 | S3 value1 |
| 8 | S3 quality |
| 9 | S4 value0 |
| 10 | S4 value1 |
| 11 | S4 quality |

Чтение всех данных:

```text
FC03
start address = 0
quantity = 12
```

Если клиент показывает адреса формата `40001`, регистр `40001` обычно соответствует protocol offset `0`.

### 10.4 Quality register

```text
bit 0      valid
bit 1      connection_lost
bit 2      physical port present
bit 3      RTU request active
bit 4      FieldSensor service paused
bits 8..15 ModbusRtuResult
```

`ModbusRtuResult`:

```text
0 idle
1 busy
2 ok
3 timeout
4 CRC error
5 invalid response
6 Modbus exception
7 transport error
8 invalid argument
```

Клиент должен проверять quality. Два value-регистра могут содержать last-good data после потери RS-485 связи.

### 10.5 Диагностика

```text
eth
```

Проверять:

```text
requests = responses
malformed = 0
transport_errors = 0
```

`exceptions` может увеличиваться при запросе неверного адреса, количества регистров или неподдерживаемой функции.

### 10.6 Расширение карты

Изменять:

```text
app/ethernet/ethernet_modbus_service.hpp
app/ethernet/ethernet_modbus_service.cpp
app/diagnostics/ethernet_status.cpp
docs/ModbusTcpExample.md
docs/README.md
```

Порядок:

1. определить новый размер map;
2. увеличить статический holding buffer;
3. заполнить новые адреса в `update_holding_map()`;
4. проверить диапазоны в `read_holding()`;
5. обновить `static_assert`;
6. обновить console-output;
7. обновить документацию;
8. протестировать минимальный и максимальный адрес.

Общая Modbus TCP function должна добавляться в `protocol/modbus_tcp/`. Прикладная карта остаётся в `app/ethernet/`. W5500 HAL не должен знать значения регистров.

[↑ К оглавлению](#contents)

---

<a id="x2x"></a>

## 11. Шина X2X

Команды:

```text
x2x
x2x reload
x2x pause
x2x resume
x2x ldo SLAVE VALUE
```

### 11.1 Формат X2X.TXT

Первая строка — количество модулей. Далее — ID в физическом порядке цепочки.

```text
4
1
2
5
6
fin
```

Получается:

```text
slave 1 -> ID 1
slave 2 -> ID 2
slave 3 -> ID 5
slave 4 -> ID 6
```

Максимум — 6 внешних модулей.

Пустая цепочка:

```text
0
fin
```

В этом случае UART0 передаётся fallback echo-test.

### 11.2 Стабильные ID

```text
0 -> LCP2116, запрещён в X2X.TXT
1 -> LDO1118
2 -> LAI1118
3 -> LDI1118
4 -> LCT1114
5 -> LDI1116
6 -> LCT1114_2
```

После публикации ID нельзя менять или повторно использовать для другого типа.

### 11.3 Диагностика

```text
x2x
```

Проверять:

```text
connection = online
communication_error = ok
success растёт
consecutive_failures = 0
uart_errors = 0
```

После отключения растёт `failed`, затем появляется:

```text
connection = lost
```

Первый корректный цикл должен автоматически восстановить:

```text
connection = online
communication_error = ok
```

### 11.4 Reload и pause

```text
x2x reload
```

Если цикл активен, новая конфигурация применяется после безопасного завершения. Невалидный новый файл не уничтожает последнюю рабочую конфигурацию.

```text
x2x pause
x2x resume
```

Pause завершает активный цикл и только затем останавливает новые опросы.

### 11.5 Проверка LDO

```text
x2x ldo 1 255
```

Команда задаёт output value LDO1118. Драйвер передаёт значение в следующем цикле. Это сервисная проверка, а не PLC-логика.

### 11.6 Данные модулей

Общий заголовок каждого X2X-модуля содержит:

- type;
- ASDU;
- slave address;
- connection state;
- device fault;
- communication result;
- exception code;
- success/failed counters;
- last update time.

Специфическая структура хранит DI, integer values, float values или output state.

[↑ К оглавлению](#contents)

---

<a id="new-x2x-module"></a>

## 12. Добавление нового X2X-модуля

Для обычного нового типа изменяются пять мест:

1. стабильный ID и runtime-структура;
2. объявление driver entry point;
3. реализация драйвера;
4. запись в каталоге;
5. проект Microchip Studio.

Обычно не изменяются:

```text
x2x_config.cpp
x2x_registry.cpp
x2x_service.cpp
x2x_status.cpp
modbus_rtu_master.cpp
```

### 12.1 ID и структура

Файл:

```text
app/x2x/x2x_types.hpp
```

Добавить стабильный ID:

```cpp
X2X_DEVICE_LTM1114 = 7U
```

Добавить runtime-структуру:

```cpp
struct X2XLtm1114 : X2XDeviceHeader
{
    static const uint8_t SP_COUNT = 1U;
    static const uint8_t ME_COUNT = 0U;
    static const uint8_t TF_COUNT = 4U;

    uint32_t digital_inputs;

    union
    {
        struct
        {
            float temperature;
            float pressure;
            float current;
            float resource;
        };

        float tf_values[TF_COUNT];
    };
};
```

Не дублировать поля `X2XDeviceHeader`: connection state, counters, exception и last update уже находятся в общем заголовке.

### 12.2 Размер статического слота

```cpp
X2X_DEVICE_SLOT_BYTES = 256U
```

`construct_device()` содержит `static_assert`. Слишком большая структура остановит сборку вместо скрытого переполнения.

Большие массивы не следует хранить в каждом экземпляре. Для LCT waveform используется отдельный общий буфер.

### 12.3 Объявление драйвера

Файл:

```text
app/x2x/modules/x2x_module_drivers.hpp
```

```cpp
X2XModulePollResult x2x_module_poll_ltm1114(
    X2XDeviceHeader* device,
    X2XModuleContext& context);
```

### 12.4 Выбор образца

Создать:

```text
app/x2x/modules/x2x_ltm.cpp
```

Выбрать ближайший существующий драйвер:

```text
x2x_ldi.cpp -> дискретные входы
x2x_lai.cpp -> DI + float
x2x_ldo.cpp -> запись выходов
x2x_lct.cpp -> многошаговый цикл и waveform
```

### 12.5 Каталог

Файл:

```text
app/x2x/x2x_catalog.cpp
```

Пример записи:

```cpp
{
    X2X_DEVICE_LTM1114,
    "LTM1114",
    sizeof(X2XLtm1114),
    X2XLtm1114::SP_COUNT,
    X2XLtm1114::ME_COUNT,
    X2XLtm1114::TF_COUNT,
    1U,
    construct_device<X2XLtm1114, X2X_DEVICE_LTM1114>,
    x2x_module_poll_ltm1114,
    print_digital_inputs_and_floats<X2XLtm1114>
}
```

Одна запись автоматически подключает тип к:

- проверке `X2X.TXT`;
- статическому registry;
- циклическому service;
- общему status;
- специфическому print callback.

Для простого DI-модуля можно использовать:

```cpp
print_digital_inputs<X2XLtm1114>
```

Для DI и массива float:

```cpp
print_digital_inputs_and_floats<X2XLtm1114>
```

Для особой структуры создаётся отдельный print callback в `x2x_catalog.cpp`.

### 12.6 Добавление в проект

```xml
<Compile Include="app\x2x\modules\x2x_ltm.cpp">
    <SubType>compile</SubType>
</Compile>
```

Либо добавить файл через Microchip Studio.

### 12.7 Конфигурация

Пример `X2X.TXT` для одного нового модуля ID 7:

```text
1
7
fin
```

Позиция строки определяет Modbus slave address. Здесь адрес равен 1.

[↑ К оглавлению](#contents)

---

<a id="x2x-driver"></a>

## 13. Разработка неблокирующего X2X-драйвера

### 13.1 Контекст драйвера

`X2XModuleContext` содержит:

```text
master
runtime
waveform
register_buffer
register_capacity
now_ms
```

Драйвер не создаёт собственный Modbus master.

Важно: `x2x_service_poll()` уже вызывает `modbus_rtu_master_poll()` перед callback активного драйвера. Драйвер не должен вызывать `modbus_rtu_master_poll()` повторно.

### 13.2 State machine

Один вызов драйвера выполняет один шаг:

```text
START_READ
  -> проверить master ready
  -> запустить запрос
  -> вернуть IN_PROGRESS

WAIT_READ
  -> проверить result
  -> если BUSY, вернуть IN_PROGRESS
  -> декодировать либо зарегистрировать ошибку
  -> reset master/runtime
  -> вернуть CYCLE_COMPLETE
```

### 13.3 Рабочий skeleton

```cpp
#include "x2x_module_drivers.hpp"
#include "x2x_module_common.hpp"

namespace
{
enum LtmPollState : uint8_t
{
    LTM_POLL_START_READ = 0U,
    LTM_POLL_WAIT_READ = 1U
};

static const uint16_t LTM_REGISTER_COUNT = 9U;
static const uint16_t LTM_FLOAT_START_REGISTER = 1U;
}

X2XModulePollResult x2x_module_poll_ltm1114(
    X2XDeviceHeader* device,
    X2XModuleContext& context)
{
    if ((device == 0) ||
        (device->type != X2X_DEVICE_LTM1114) ||
        (context.master == 0) ||
        (context.runtime == 0) ||
        (context.register_buffer == 0) ||
        (context.register_capacity < LTM_REGISTER_COUNT))
    {
        return X2X_MODULE_POLL_CYCLE_COMPLETE;
    }

    X2XLtm1114* ltm = static_cast<X2XLtm1114*>(device);

    switch (context.runtime->state)
    {
        case LTM_POLL_START_READ:
            if (modbus_rtu_master_ready(*context.master) == 0U)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            if (modbus_rtu_master_start_read_holding(
                    *context.master,
                    device->slave_address,
                    0U,
                    LTM_REGISTER_COUNT,
                    context.register_buffer,
                    X2X_MODBUS_TRANSACTION_TIMEOUT_MS) == 0U)
            {
                x2x_module_mark_failure(
                    *device,
                    modbus_rtu_master_result(*context.master),
                    modbus_rtu_master_exception_code(*context.master));
                modbus_rtu_master_reset(*context.master);
                context.runtime->state = LTM_POLL_START_READ;
                return X2X_MODULE_POLL_CYCLE_COMPLETE;
            }

            context.runtime->state = LTM_POLL_WAIT_READ;
            return X2X_MODULE_POLL_IN_PROGRESS;

        case LTM_POLL_WAIT_READ:
        {
            const ModbusRtuResult result =
                modbus_rtu_master_result(*context.master);

            if (result == MODBUS_RTU_RESULT_BUSY)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            if (result == MODBUS_RTU_RESULT_OK)
            {
                ltm->digital_inputs = context.register_buffer[0];
                x2x_module_decode_float_array(
                    ltm->tf_values,
                    X2XLtm1114::TF_COUNT,
                    context.register_buffer,
                    LTM_FLOAT_START_REGISTER,
                    LTM_REGISTER_COUNT);
                x2x_module_mark_success(*device, context.now_ms);
            }
            else
            {
                x2x_module_mark_failure(
                    *device,
                    result,
                    modbus_rtu_master_exception_code(*context.master));
            }

            modbus_rtu_master_reset(*context.master);
            context.runtime->state = LTM_POLL_START_READ;
            return X2X_MODULE_POLL_CYCLE_COMPLETE;
        }

        default:
            modbus_rtu_master_reset(*context.master);
            context.runtime->state = LTM_POLL_START_READ;
            return X2X_MODULE_POLL_CYCLE_COMPLETE;
    }
}
```

Адреса и размер запроса должны соответствовать фактической карте регистров модуля.

### 13.4 Float word order

Штатные helpers используют:

```text
первый регистр  -> младшее 16-битное слово
второй регистр -> старшее 16-битное слово
```

```cpp
x2x_module_registers_to_float(...)
x2x_module_decode_float_array(...)
```

Если новый модуль имеет другой word order, реализовать преобразование только в его драйвере. Нельзя глобально менять helper и ломать существующие типы.

### 13.5 Success и failure

Успешный обмен:

```cpp
x2x_module_mark_success(*device, context.now_ms);
```

Ошибка:

```cpp
x2x_module_mark_failure(
    *device,
    modbus_rtu_master_result(*context.master),
    modbus_rtu_master_exception_code(*context.master));
```

### 13.6 Проверка нового драйвера

1. собрать Debug и Release;
2. создать `X2X.TXT` с новым ID;
3. выполнить `x2x reload`;
4. проверить имя, SP/ME/TF;
5. проверить рост `success`;
6. изменить реальные входы/выходы;
7. отключить линию;
8. проверить `connection = lost`;
9. восстановить линию;
10. проверить automatic recovery;
11. выполнить `rtos` до и после длительного теста;
12. описать register map и word order в Markdown.

[↑ К оглавлению](#contents)

---

<a id="new-console-command"></a>

## 14. Добавление console-команды

Основной диспетчер:

```text
app/diagnostics/diagnostic_console.cpp
```

Профильные команды лучше добавлять в handler соответствующей подсистемы:

```text
field_status_handle_command()
x2x_status_handle_command()
ethernet_status_handle_command()
rtc_status_handle_command()
watchdog_status_handle_command()
```

Порядок:

1. реализовать действие в профильном service;
2. добавить handler;
3. добавить запись в `help`;
4. добавить короткий диагностический ответ;
5. проверить неизвестную и некорректную форму команды;
6. убедиться, что команда не блокирует основную task.

Правила вывода:

```text
name = value
name = value, next = value
0: value, 1: value
```

Использовать:

```cpp
diagnostic_print_assignment(...)
diagnostic_print_index(...)
diagnostic_print_banner(...)
diagnostic_print_section(...)
diagnostic_print_group(...)
```

Каждая строка должна по возможности помещаться в терминал шириной около 80 символов.

[↑ К оглавлению](#contents)

---

<a id="troubleshooting"></a>

## 15. Типовые неисправности

### 15.1 Modbus RTU result

```text
idle             запрос отсутствует
busy             транзакция выполняется
ok               корректный ответ
timeout          ответ не получен в срок
CRC error        неверный CRC
invalid response неверный адрес, функция, длина или данные
exception        slave вернул Modbus exception
transport error  ошибка UART/TX
invalid argument неверные параметры запуска
```

### 15.2 `request = busy` и `last_result = ok`

Это нормальное состояние:

- предыдущий запрос завершился успешно;
- следующий запрос уже выполняется.

### 15.3 `consecutive_failures = 255`

Счётчик насыщается на 255 и не переполняется. Это означает длительное отсутствие корректных ответов.

### 15.4 Значения есть, но `valid = no`

Прошивка сохраняет last-good values. Прикладная логика обязана учитывать `valid` и `connection_lost`.

### 15.5 X2X exception code 2

Обычно это `Illegal Data Address`.

Проверить:

1. ID в `X2X.TXT`;
2. версию платы и firmware модуля;
3. start address и register count драйвера;
4. фактическую register map.

### 15.6 Ethernet `link = down`

Проверить:

- кабель;
- коммутатор;
- питание W5500;
- правильный разъём ETH1/ETH2;
- `VERSIONR = 0x04`;
- socket state после подключения.

### 15.7 microSD file not found

Проверить:

- файл находится в корне;
- точный регистр имени;
- расширение `.TXT` или `.txt`;
- FAT16/FAT32;
- отсутствие двойного расширения, например `.txt.txt`.

### 15.8 Неожиданный reset

```text
watchdog
uptime
```

Проверить reset cause и boot count.

### 15.9 Подозрение на утечку памяти

Повторить несколько раз:

```text
status
rtos
```

Сравнить:

- current free heap;
- minimum-ever-free heap;
- task stack high-water mark.

Current free heap не должен уменьшаться после каждого одинакового цикла.

### 15.10 Console не принимает команды

Проверить:

- правильный USB CDC-порт;
- окончание команды Enter;
- отсутствие непрерывного `periodic on` при медленном терминале;
- отсутствие аппаратного reset;
- состояние `uptime` после переподключения.

[↑ К оглавлению](#contents)

---

<a id="development-rules"></a>

## 16. Правила разработки

### 16.1 Неблокирующая работа

Основная task обслуживает все подсистемы. Запрещены длительные блокировки.

Не использовать:

```text
delay
busy-wait
while до аппаратного события
ожидание ответа без timeout
неограниченный цикл обработки входного буфера
```

Длительная операция должна иметь:

- state;
- start function;
- poll function;
- timeout;
- явный result.

### 16.2 Память

Предпочтительно:

- статические буферы фиксированного размера;
- compile-time `static_assert`;
- отсутствие динамического выделения в runtime;
- проверка границ до записи;
- сохранение last-good data отдельно от quality.

### 16.3 Стиль C/C++

Настройки:

```text
.clang-format
.editorconfig
```

Основные правила:

- 4 пробела;
- tab запрещён;
- Allman braces;
- пробелы вокруг бинарных операторов;
- короткие функции и условия не сворачиваются в одну строку;
- строка исходника до 100 символов, когда это возможно.

### 16.4 Стиль console-output

```text
name = value
name = value, next = value
0: value, 1: value
```

Не создавать новый формат вручную в каждом модуле.

### 16.5 Изменение архитектуры

Перед изменением общего protocol engine проверить, можно ли решить задачу на уровне service или конкретного драйвера.

Обычно новый X2X-модуль не требует изменений:

```text
x2x_config.cpp
x2x_registry.cpp
x2x_service.cpp
x2x_status.cpp
modbus_rtu_master.cpp
```

Новый внешний прибор не должен заставлять `ModbusRtuMaster` знать его модель.

[↑ К оглавлению](#contents)

---

<a id="release-checklist"></a>

## 17. Проверка перед выпуском

### 17.1 Сборка

- Debug собирается;
- Release собирается;
- нет ошибок линковки;
- warnings просмотрены;
- Flash и SRAM зафиксированы;
- новый `.cpp` включён в проект.

### 17.2 Smoke test

```text
version
help
status
uptime
rtos
sd
field
x2x
eth
time
watchdog
```

### 17.3 FieldSensor

- S1–S4 обнаружены;
- параметры SD применены;
- подключённый прибор переходит в `online`;
- `success` растёт;
- отключение формирует `lost`;
- восстановление автоматическое;
- последние корректные значения сохраняются;
- quality меняется правильно.

### 17.4 Ethernet

- ETH1 и ETH2 инициализируются;
- `VERSIONR = 0x04`;
- link поднимается после подключения;
- FC03 читает карту;
- `requests = responses`;
- `malformed = 0`;
- `transport_errors = 0`;
- после disconnect/reconnect сервер восстанавливается.

### 17.5 X2X

- `X2X.TXT` загружается;
- registry создаётся;
- правильные slave и type ID;
- подключённые модули переходят в `online`;
- данные изменяются вместе с физическими входами/выходами;
- timeout и exception обрабатываются;
- восстановление автоматическое;
- UART errors отсутствуют.

### 17.6 RTC и watchdog

- `time` показывает корректное время;
- `rtc set` не блокирует другие service;
- watchdog test reset перезагружает устройство;
- после reset USB CDC восстанавливается.

### 17.7 Длительный прогон

Во время прогона:

- uptime не сбрасывается;
- heap стабилен;
- stack high-water mark стабилен;
- counters продолжают расти;
- отсутствуют спонтанные reset;
- не увеличиваются transport errors без внешней причины.

[↑ К оглавлению](#contents)

---

<a id="documentation-map"></a>

## 18. Карта документации

Этот `README.md` является основным объединённым руководством. Исходные специализированные документы сохранены, чтобы разработчик мог открыть узкую тему отдельно.

### 18.1 Эксплуатация и расширение

| Файл | Назначение |
|---|---|
| [FirmwareGuide.md](FirmwareGuide.md) | Первоначальный общий эксплуатационный и разработческий гайд |
| [FieldSensorExample.md](FieldSensorExample.md) | Подробный пример FieldSensor на S1–S4 |
| [ModbusTcpExample.md](ModbusTcpExample.md) | Modbus TCP, сетевые файлы и holding map |
| [X2XRuntimeControl.md](X2XRuntimeControl.md) | Команды reload, pause, resume и LDO |
| [AddingX2XModule.md](AddingX2XModule.md) | Пошаговое добавление нового X2X-типа |
| [LorentzBasicFirmwareScope.md](LorentzBasicFirmwareScope.md) | Границы и назначение baseline firmware |

### 18.2 Проверка и качество

| Файл | Назначение |
|---|---|
| [ReleaseVerification.md](ReleaseVerification.md) | Зафиксированный объём аппаратной проверки release |
| [CodeQualityReview.md](CodeQualityReview.md) | Результаты статического review и архитектурные решения |
| [FreeRTOSBaselineTest.md](FreeRTOSBaselineTest.md) | План базового теста FreeRTOS |
| [X2XBaselineTest.md](X2XBaselineTest.md) | План проверки X2X |
| [X2XHardwareResults.md](X2XHardwareResults.md) | Аппаратные результаты X2X |

### 18.3 LCT

| Файл | Назначение |
|---|---|
| [LCTRegisterMaps.md](LCTRegisterMaps.md) | Карты регистров версий LCT |
| [LCTModuleFirmwareIssues.md](LCTModuleFirmwareIssues.md) | Обнаруженные особенности firmware LCT |
| [LCTModuleFirmwareFixes.patch](LCTModuleFirmwareFixes.patch) | Patch с предложенными исправлениями module firmware |

### 18.4 Почему исходные файлы не удалены

Единый README удобен для чтения и передачи проекта. Специализированные файлы удобны для:

- точечных изменений одной подсистемы;
- code review;
- истории аппаратных проверок;
- ссылок из commit и issue;
- исключения огромного diff при локальном обновлении одной главы.

При изменении функциональности сначала обновляется профильный документ, затем соответствующая глава этого README.

[↑ К оглавлению](#contents)

---

## Статус документа

Документ объединяет эксплуатационные и разработческие сведения для `LCP Basic Firmware 1.00.0`.

Титульный блок и изображения можно оформить позднее без изменения структуры глав. Рекомендуется хранить изображения в:

```text
00_LCP2116/RTOS/docs/assets/
```

и подключать относительными путями:

```html
<img src="assets/lorentz-logo.svg" alt="Lorentz" width="240">
```

