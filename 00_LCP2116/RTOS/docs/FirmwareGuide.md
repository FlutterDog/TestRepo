# LCP Basic Firmware — руководство пользователя и разработчика

## Содержание

1. [Назначение прошивки](#1-назначение-прошивки)
2. [Архитектура проекта](#2-архитектура-проекта)
3. [Сборка и запуск](#3-сборка-и-запуск)
4. [Подготовка microSD](#4-подготовка-microsd)
5. [USB service console](#5-usb-service-console)
6. [Команды и диагностика](#6-команды-и-диагностика)
7. [FieldSensor S1–S4](#7-fieldsensor-s1s4)
8. [Подключение другого Modbus RTU-прибора](#8-подключение-другого-modbus-rtu-прибора)
9. [X2X](#9-x2x)
10. [Добавление нового X2X-модуля](#10-добавление-нового-x2x-модуля)
11. [Ethernet и Modbus TCP](#11-ethernet-и-modbus-tcp)
12. [Добавление console-команды](#12-добавление-console-команды)
13. [Типовые неисправности](#13-типовые-неисправности)
14. [Правила разработки](#14-правила-разработки)
15. [Release checklist](#15-release-checklist)

---

## 1. Назначение прошивки

`LCP Basic Diagnostic Firmware` — базовая прошивка контроллера LCP2116. Она предназначена для:

- проверки аппаратной части контроллера;
- работы с внутренней шиной X2X;
- опроса внешних Modbus RTU-приборов через S1–S4;
- публикации данных через два Ethernet-интерфейса;
- диагностики microSD, RTC, watchdog, UART и памяти;
- использования как основы для прикладного проекта.

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
- диагностические команды.

В baseline не входят:

- PNX Studio;
- прикладная PLC-логика установки;
- IEC 60870-5-104;
- операторская панель;
- универсальная база внешних приборов;
- автоматическое применение `EIA.TXT`.

Doxygen-комментарии объясняют отдельные функции и структуры. Этот документ описывает общий рабочий процесс и точки расширения.

---

## 2. Архитектура проекта

### 2.1 Поток выполнения

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

Все прикладные service работают неблокирующе внутри одной задачи. Поэтому любая функция, которая долго ждёт аппаратное событие, задерживает остальные подсистемы.

### 2.2 Каталоги

```text
app/
    прикладные service, runtime-состояния и диагностика

board/
    привязка логических интерфейсов к плате LCP2116

hal/
    ATSAM3X8E, SC16IS7xx, W5500 и другая низкоуровневая периферия

protocol/
    универсальные Modbus RTU и Modbus TCP engine

platform/
    Serial, SPI, GPIO, millis и совместимые platform-функции

libs/
    отдельные reusable-библиотеки

sd_card/
    штатные конфигурационные файлы для microSD

docs/
    эксплуатационные и разработческие документы
```

### 2.3 Владение интерфейсами

Один физический интерфейс должен иметь одного владельца.

```text
UART0       -> X2X master
                либо fallback echo при пустой X2X-конфигурации

S1..S4      -> FieldSensor Modbus RTU master

ETH1        -> отдельный Modbus TCP server
ETH2        -> отдельный Modbus TCP server

PC/HMI UART -> diagnostic echo до добавления реального протокола
```

Нельзя одновременно запускать два protocol engine на одном half-duplex UART.

---

## 3. Сборка и запуск

Проект Microchip Studio:

```text
00_LCP2116/RTOS/LCP_Basic.cppproj
```

Target:

```text
ATSAM3X8E
ARM Cortex-M3
```

Рекомендуемый порядок:

1. открыть `LCP_Basic.cppproj`;
2. собрать `Debug`;
3. собрать `Release`;
4. проверить Build Output;
5. проверить Flash и SRAM;
6. прошить Release-бинарник;
7. открыть USB CDC console;
8. выполнить минимальный тест.

Проверить в Build Output:

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

Стандартное предупреждение Atmel Studio о приблизительной оценке memory usage не является warning исходного кода.

После добавления нового `.cpp` его необходимо включить в `LCP_Basic.cppproj` через Microchip Studio либо вручную.

---

## 4. Подготовка microSD

Поддерживается FAT16/FAT32. Примеры находятся в:

```text
00_LCP2116/RTOS/sd_card/
```

Файлы копируются в корень карты.

### 4.1 FieldSensor

```text
baud.TXT
Parity.TXT
```

### 4.2 Ethernet

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

### 4.3 X2X

```text
X2X.TXT
```

### 4.4 Зарезервированный файл

```text
EIA.TXT
```

Baseline сохраняет его в комплекте, но runtime его не применяет.

### 4.5 Точные имена

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

---

## 5. USB service console

Прошивка создаёт USB CDC-порт. Можно использовать PuTTY, Tera Term, RealTerm или другой serial terminal.

Рекомендуемые настройки:

```text
115200
8 data bits
no parity
1 stop bit
no flow control
```

Для USB CDC выбранный baudrate не определяет физическую скорость, но 115200 удобно использовать как стандарт.

После подключения:

```text
LCP firmware ready. Type 'help' or '?'.
```

Команды не чувствительны к регистру. Выполнение — Enter.

### 5.1 Формат вывода

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

---

## 6. Команды и диагностика

Первая команда:

```text
help
```

Alias:

```text
?
```

### 6.1 Основные команды

| Команда | Назначение |
|---|---|
| `help`, `?` | Список команд по группам |
| `version`, `ver` | Версия, stage и target |
| `status` | Полный отчёт всех подсистем |
| `uptime` | Время работы в ms и привычном формате |
| `rtos` | SRAM, heap и stack в байтах и процентах |

`status` удобен для полного снимка. Для ежедневной диагностики лучше использовать отдельные команды `field`, `x2x`, `eth`, `rtc` и другие.

### 6.2 Периодический отчёт

```text
periodic on
periodic off
```

`periodic on` печатает полный `status` каждые 10 секунд. Использовать только при целенаправленной диагностике.

### 6.3 microSD

```text
sd
sd test
```

`sd` показывает наличие карты, FAT и состояние filesystem.

`sd test` создаёт и читает `SDTEST.TXT`. Команда изменяет содержимое карты.

### 6.4 RTC

Короткий вывод:

```text
time
rtc time
```

Полный отчёт:

```text
rtc
```

Установка:

```text
rtc set 2026-07-20 21:30:00
```

Операция неблокирующая. После неё другие service продолжают работу.

### 6.5 Watchdog

```text
watchdog
watchdog feed
watchdog test reset
```

`watchdog test reset` намеренно прекращает автоматический feed. Контроллер должен перезагрузиться.

### 6.6 UART и SC16IS

```text
rs485
sc16is
```

`rs485` показывает владельца, format и hardware errors каждого UART.

`sc16is` показывает обнаруженную аппаратную конфигурацию внешних UART и назначение PC, HMI и S1.

`SC16.txt` не используется: микросхемы определяются автоматически.

### 6.7 Проверка памяти

```text
rtos
```

Нормальное поведение:

- current free heap не уменьшается при повторении одинаковых действий;
- minimum-ever-free после начального прогона стабилизируется;
- stack имеет запас;
- uptime не сбрасывается без причины.

---

## 7. FieldSensor S1–S4

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

### 7.1 Физическое соответствие

```text
S1 -> SC16IS7xx
S2 -> ATSAM3X8E UART1 / Serial1
S3 -> ATSAM3X8E UART3 / Serial3
S4 -> ATSAM3X8E UART2 / Serial2
```

### 7.2 baud.TXT

```text
4
9600
9600
1200
9600
fin
```

Порядок значений:

```text
S1
S2
S3
S4
```

Это фактические baudrate, а не индексы таблицы.

### 7.3 Parity.TXT

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

### 7.4 Команда field

Для каждого порта выводятся:

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
- время последнего обновления.

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

Последние корректные значения сохраняются. Их можно использовать только при `valid = yes`.

### 7.5 Reload и pause

```text
field reload
```

Повторно читает `baud.TXT` и `Parity.TXT`. Активные транзакции сначала завершаются, затем UART переинициализируются.

```text
field pause
field resume
```

Pause запрещает новые запросы, но не обрывает уже активный кадр.

---

## 8. Подключение другого Modbus RTU-прибора

### 8.1 Где задаётся прибор

Файл:

```text
app/field/field_sensor_service.cpp
```

Функция:

```cpp
set_default_configs()
```

Параметры:

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

### 8.2 Размер буфера

Текущий размер:

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

### 8.3 Когда нужен отдельный service

Изменить baseline достаточно для простого прибора с одним FC03-запросом.

Создать отдельный service рекомендуется, если прибор требует:

- нескольких запросов;
- FC10 или других операций записи;
- собственной state machine;
- особого декодирования;
- отдельных quality-флагов;
- собственной диагностики.

`ModbusRtuMaster` должен оставаться универсальным protocol engine и не знать модель устройства.

### 8.4 Неблокирующий шаблон

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

---

## 9. X2X

Команды:

```text
x2x
x2x reload
x2x pause
x2x resume
x2x ldo SLAVE VALUE
```

### 9.1 X2X.TXT

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

Тогда UART0 передаётся fallback echo-test.

### 9.2 Стабильные ID

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

### 9.3 Диагностика

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

После отключения растёт `failed`, затем появляется `connection = lost`. Первый корректный цикл должен восстановить `connection = online`.

### 9.4 Reload и pause

```text
x2x reload
```

Если цикл активен, новая конфигурация применяется после безопасного завершения. Невалидный новый файл не должен уничтожать последнюю рабочую конфигурацию.

```text
x2x pause
x2x resume
```

Pause завершает активный цикл и только затем останавливает опрос.

### 9.5 LDO

```text
x2x ldo 1 255
```

Команда задаёт output value LDO1118. Драйвер передаст значение в следующем цикле. Это сервисная проверка, а не PLC-логика.

---

## 10. Добавление нового X2X-модуля

Для обычного нового типа изменяются пять мест:

1. ID и runtime-структура;
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

### 10.1 ID и структура

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

### 10.2 Размер статического слота

```cpp
X2X_DEVICE_SLOT_BYTES = 256U
```

`construct_device()` содержит `static_assert`. Слишком большая структура остановит сборку вместо скрытого переполнения.

Большие массивы не следует хранить в каждом экземпляре. Для LCT waveform используется отдельный общий буфер.

### 10.3 Объявление драйвера

Файл:

```text
app/x2x/modules/x2x_module_drivers.hpp
```

```cpp
X2XModulePollResult x2x_module_poll_ltm1114(
    X2XDeviceHeader* device,
    X2XModuleContext& context);
```

### 10.4 Выбор образца

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

### 10.5 Контекст драйвера

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

### 10.6 Неблокирующая state machine

Один вызов драйвера выполняет только один шаг:

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

### 10.7 Рабочий skeleton

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

Адреса и размер запроса заменить согласно реальной register map.

### 10.8 Float word order

Штатные helpers используют:

```text
первый регистр  -> младшее 16-битное слово
второй регистр -> старшее 16-битное слово
```

```cpp
x2x_module_registers_to_float(...)
x2x_module_decode_float_array(...)
```

Если новый модуль имеет другой word order, реализовать это только в его драйвере. Нельзя глобально менять helper и ломать существующие типы.

### 10.9 Каталог

Файл:

```text
app/x2x/x2x_catalog.cpp
```

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

Одна запись подключает тип к config validation, registry, service и console.

Для особого diagnostic output создать отдельный print callback в `x2x_catalog.cpp`.

### 10.10 Добавление в проект

```xml
<Compile Include="app\x2x\modules\x2x_ltm.cpp">
    <SubType>compile</SubType>
</Compile>
```

Либо добавить файл через Microchip Studio.

### 10.11 Проверка нового модуля

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

Подробная отдельная инструкция: [AddingX2XModule.md](AddingX2XModule.md).

---

## 11. Ethernet и Modbus TCP

Оба W5500 запускают независимые server instance.

```text
protocol = Modbus TCP
port = 502
function = FC03 Read Holding Registers
holding = 0..11
write functions = not supported
```

Оба интерфейса публикуют одну и ту же карту FieldSensor.

### 11.1 Файлы

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

### 11.2 Команды

```text
eth
eth reload
```

Ожидаемое состояние подключённого интерфейса:

```text
initialized = yes
init = ok
VERSIONR = 0x04
link = up
```

### 11.3 Holding map

```text
0   S1 value0
1   S1 value1
2   S1 quality

3   S2 value0
4   S2 value1
5   S2 quality

6   S3 value0
7   S3 value1
8   S3 quality

9   S4 value0
10  S4 value1
11  S4 quality
```

Чтение всей карты:

```text
FC03
start address = 0
quantity = 12
```

### 11.4 Quality

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

Клиент обязан учитывать quality. `value0` и `value1` могут содержать last-good data после потери связи.

### 11.5 Диагностика

```text
requests = responses
malformed = 0
transport_errors = 0
```

`exceptions` увеличивается при неверном адресе, количестве регистров или неподдерживаемой функции.

### 11.6 Расширение карты

Изменять:

```text
app/ethernet/ethernet_modbus_service.hpp
app/ethernet/ethernet_modbus_service.cpp
app/diagnostics/ethernet_status.cpp
docs/ModbusTcpExample.md
docs/FirmwareGuide.md
```

Порядок:

1. определить новый размер;
2. увеличить holding buffer;
3. заполнить значения в `update_holding_map()`;
4. проверить диапазоны `read_holding()`;
5. обновить `static_assert`;
6. обновить console;
7. обновить документацию;
8. проверить минимальный и максимальный адрес.

MBAP и общие function code находятся в `protocol/modbus_tcp/`. Прикладная карта находится в `app/ethernet/`. W5500 HAL не должен знать структуру FieldSensor.

Подробности: [ModbusTcpExample.md](ModbusTcpExample.md).

---

## 12. Добавление console-команды

Основной файл:

```text
app/diagnostics/diagnostic_console.cpp
```

Профильные команды лучше добавлять в handler подсистемы:

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
4. добавить diagnostic output;
5. проверить неправильный синтаксис;
6. убедиться, что команда не блокирует task.

Использовать formatter:

```cpp
diagnostic_print_assignment(...)
diagnostic_print_index(...)
diagnostic_print_banner(...)
diagnostic_print_section(...)
diagnostic_print_group(...)
```

Не создавать новый стиль вывода вручную в каждом модуле.

---

## 13. Типовые неисправности

### 13.1 `file not found`

Проверить:

- файл в корне microSD;
- точный регистр имени;
- правильное расширение;
- отсутствие `.txt.txt`;
- FAT16/FAT32.

### 13.2 FieldSensor timeout

Проверить:

- физический порт;
- slave address;
- baudrate/parity;
- A/B RS-485;
- питание прибора;
- start register/count;
- `uart_errors`.

### 13.3 `request = busy`, `last_result = ok`

Это нормально: предыдущий запрос завершился успешно, следующий уже запущен.

### 13.4 `consecutive_failures = 255`

Счётчик насыщается и не переполняется. Это длительное отсутствие корректных ответов.

### 13.5 Значения есть, но `valid = no`

Это last-good data. Использовать значения нельзя без проверки quality.

### 13.6 X2X exception code 2

Обычно `Illegal Data Address`. Проверить:

1. ID в `X2X.TXT`;
2. версию module firmware;
3. register map драйвера;
4. start/count.

### 13.7 Ethernet link down

Проверить:

- кабель и switch;
- правильный разъём;
- питание W5500;
- `VERSIONR = 0x04`;
- IP/subnet;
- повторный `eth reload`.

### 13.8 Неожиданный reset

```text
watchdog
uptime
```

Проверить reset cause и boot count.

### 13.9 Подозрение на утечку памяти

Несколько раз выполнить:

```text
status
rtos
```

Сравнить current free heap, minimum-ever-free и stack high-water mark.

---

## 14. Правила разработки

### 14.1 Неблокирующие service

Не использовать:

```text
delay
busy-wait
while до аппаратного события
ожидание без timeout
неограниченную обработку входного буфера
```

Длительная операция должна иметь:

- state;
- start;
- poll;
- timeout;
- result.

### 14.2 Память

Предпочтительно:

- статические буферы;
- `static_assert`;
- проверка границ;
- отсутствие runtime allocation;
- last-good data отдельно от quality.

### 14.3 Разделение уровней

```text
HAL       -> аппаратные регистры и операции
board     -> разводка LCP2116
protocol  -> универсальный протокол
service   -> конкретное применение протокола
status    -> диагностика и команды
```

Неправильно:

- W5500 HAL знает карту FieldSensor;
- Modbus master знает модель прибора;
- X2X-драйвер читает microSD;
- console обходит service и напрямую меняет protocol state.

### 14.4 Стиль

```text
.clang-format
.editorconfig
```

Основные правила:

- 4 пробела;
- без tab;
- Allman braces;
- пробелы вокруг бинарных операторов;
- строка исходника до 100 символов, когда возможно.

### 14.5 Документация

При изменении внешнего формата, команды или register map обновить Markdown в том же commit.

---

## 15. Release checklist

Перед release:

1. Debug build успешен;
2. Release build успешен;
3. warnings отсутствуют либо объяснены;
4. Flash/SRAM в пределах ATSAM3X8E;
5. версия корректна;
6. microSD-файлы проверены;
7. S1–S4 проверены;
8. ETH1 и ETH2 проверены;
9. X2X проверен на реальных модулях;
10. RTC и watchdog проверены;
11. heap/stack имеют запас;
12. документация обновлена;
13. release commit/tag создан;
14. ветка объединена с `main`.

Минимальные команды после функционального изменения:

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

---

## Связанные документы

- [AddingX2XModule.md](AddingX2XModule.md) — отдельная инструкция добавления X2X-типа;
- [FieldSensorExample.md](FieldSensorExample.md) — текущий пример S1–S4;
- [ModbusTcpExample.md](ModbusTcpExample.md) — Ethernet-файлы и карта;
- [X2XRuntimeControl.md](X2XRuntimeControl.md) — команды управления X2X;
- [X2XBaselineTest.md](X2XBaselineTest.md) — аппаратный тест X2X;
- [ReleaseVerification.md](ReleaseVerification.md) — зафиксированные результаты проверки;
- [CodeQualityReview.md](CodeQualityReview.md) — архитектурные решения code review.
