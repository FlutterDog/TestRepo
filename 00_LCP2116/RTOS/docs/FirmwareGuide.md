# LCP Basic Firmware — руководство пользователя и разработчика

## 1. Назначение документа

Этот документ описывает практическую работу с базовой прошивкой контроллера LCP2116:

- сборку и запуск проекта;
- подготовку microSD;
- работу с USB service console;
- диагностику RTOS, microSD, RTC, watchdog, RS-485, X2X и Ethernet;
- подключение внешних Modbus RTU-приборов к S1–S4;
- чтение данных через Modbus TCP;
- добавление нового X2X-модуля;
- разработку неблокирующего драйвера;
- расширение FieldSensor и Ethernet-карты.

Документ рассчитан на инженера, который получил исходный проект и должен:

1. собрать и проверить базовую прошивку;
2. понять, где находится каждая подсистема;
3. добавить новый модуль или прибор без переработки всей архитектуры.

Doxygen-комментарии описывают отдельные функции и структуры. Этот файл объясняет общий порядок действий и связи между файлами.

---

## 2. Область действия базовой прошивки

Текущая базовая прошивка содержит:

- FreeRTOS;
- одну основную прикладную задачу LCP;
- внутреннюю шину X2X на UART0;
- четыре независимых Modbus RTU master на S1–S4;
- два независимых W5500;
- два Modbus TCP server на порту 502;
- microSD/FAT;
- USB CDC service console;
- локальный RTC;
- hardware watchdog;
- аппаратную и программную диагностику.

В baseline не входят:

- прикладная PLC-логика конкретной установки;
- PNX Studio;
- IEC 60870-5-104;
- база произвольных внешних приборов;
- логика операторской панели;
- автоматическая маршрутизация каналов из `EIA.TXT`.

Baseline — это рабочая основа и набор примеров. Прикладные функции добавляются поверх существующих service, protocol, board и HAL-слоёв.

---

## 3. Архитектура проекта

Основная структура:

```text
main.cpp
  -> app_rtos_start()
      -> LCP FreeRTOS task
          -> setup
          -> постоянный loop
              -> microSD
              -> X2X
              -> FieldSensor S1..S4
              -> Ethernet ETH1/ETH2
              -> RTC
              -> watchdog
              -> USB console
```

Каталоги:

```text
app/
    прикладные service, runtime-состояния и диагностика

board/
    привязка логических интерфейсов к конкретной плате LCP2116

hal/
    низкоуровневая работа ATSAM3X8E, SC16IS7xx и W5500

protocol/
    независимые protocol engine Modbus RTU и Modbus TCP

platform/
    совместимые Serial, SPI, GPIO, millis и другие базовые функции

libs/
    отдельные reusable-библиотеки, например microSD storage

sd_card/
    штатный комплект конфигурационных файлов для копирования на microSD

docs/
    руководства, отчёты проверки и описания подсистем
```

### Главное архитектурное правило

Один физический интерфейс должен иметь одного владельца.

Примеры:

```text
UART0       -> X2X master либо fallback echo при пустой X2X-конфигурации
S1..S4      -> FieldSensor Modbus RTU master
ETH1        -> отдельный Modbus TCP server
ETH2        -> отдельный Modbus TCP server
PC/HMI UART -> diagnostic echo до появления реального протокола
```

Нельзя одновременно запускать два protocol engine на одном half-duplex UART.

---

## 4. Сборка проекта

Проект Microchip Studio:

```text
00_LCP2116/RTOS/LCP_Basic.cppproj
```

Целевая микросхема:

```text
ATSAM3X8E
Cortex-M3
```

Рекомендуемый порядок:

1. открыть `LCP_Basic.cppproj` в Microchip Studio;
2. выбрать `Debug` и выполнить Build;
3. выбрать `Release` и выполнить Build;
4. проверить отсутствие ошибок компиляции и линковки;
5. проверить итоговый размер Flash и SRAM;
6. прошить Release-бинарник;
7. выполнить базовый console-test.

Для поставки используется Release-конфигурация.

После изменения состава исходников новый `.cpp` необходимо добавить в `LCP_Basic.cppproj`. Заголовки, подключённые через `#include`, компилируются автоматически, но их желательно также добавить в дерево проекта для удобства навигации.

### Что проверить в Build Output

```text
Build succeeded
0 failed
```

Также зафиксировать:

```text
text / Flash
.data
.bss / SRAM
warnings
```

Стандартное сообщение Atmel Studio о приблизительной оценке memory usage не является warning исходного кода.

---

## 5. Подготовка microSD

Поддерживается FAT16/FAT32. Штатные образцы находятся в:

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

`X2X.TXT` создаётся в соответствии с фактической цепочкой модулей.

### 5.4 Зарезервированный файл

```text
EIA.TXT
```

Файл сохраняется для совместимости с прикладными проектами, но базовая прошивка его не применяет.

### 5.5 Строгое соблюдение регистра имени

Имена файлов канонические. Например:

```text
baud.TXT
Parity.TXT
IP.txt
```

Не следует создавать варианты:

```text
BAUD.TXT
PARITY.TXT
IP.TXT
```

Псевдонимы намеренно не поддерживаются.

---

## 6. Подключение к USB service console

Прошивка создаёт USB CDC-порт.

Подойдут:

- PuTTY;
- Tera Term;
- RealTerm;
- любой serial terminal.

Рекомендуемая настройка терминала:

```text
115200
8 data bits
no parity
1 stop bit
no flow control
```

Для USB CDC номинальный baudrate не влияет на физическую скорость обмена, но значение 115200 удобно использовать как стандартное.

После открытия порта должно появиться сообщение:

```text
LCP firmware ready. Type 'help' or '?'.
```

Команды не чувствительны к регистру. Выполнение — клавишей Enter.

---

## 7. Начало работы с console

Первая команда:

```text
help
```

Короткий alias:

```text
?
```

Проверка версии:

```text
version
```

или:

```text
ver
```

Общий диагностический отчёт:

```text
status
```

`status` выводит все подсистемы. Для обычной работы удобнее использовать отдельные короткие команды, поскольку полный отчёт получается большим.

---

## 8. Справочник команд

## 8.1 Основные команды

### `help` / `?`

Показывает сгруппированный список команд.

### `version` / `ver`

Показывает:

- имя прошивки;
- версию;
- stage;
- target MCU.

### `status`

Показывает полный отчёт:

- system;
- RTOS;
- RS-485;
- X2X;
- FieldSensor;
- SC16IS;
- Ethernet;
- microSD;
- battery;
- RTC;
- watchdog.

### `uptime`

Показывает:

```text
uptime_ms = 12345678
uptime_human = 0 d 03:25:45
```

`uptime_ms` нужен для отладки, `uptime_human` — для человека.

### `rtos`

Показывает:

- полный SRAM;
- assigned/unassigned linker memory;
- `.data` и `.bss`;
- FreeRTOS heap;
- текущий и минимальный свободный heap;
- размер и peak usage основной task stack;
- значения в байтах и процентах.

Признаки нормальной работы:

- heap не уменьшается при повторении одинаковых команд;
- minimum-ever-free после начального прогона стабилизируется;
- task stack имеет достаточный запас;
- uptime не сбрасывается самопроизвольно.

## 8.2 Periodic report

```text
periodic on
periodic off
```

`periodic on` печатает полный `status` каждые 10 секунд.

Использовать только при целенаправленной диагностике. Постоянная печать большого отчёта создаёт дополнительную нагрузку на USB console.

## 8.3 microSD

```text
sd
sd test
```

`sd` показывает:

- наличие карты;
- тип FAT;
- готовность filesystem;
- состояние тестового файла.

`sd test` создаёт и читает `SDTEST.TXT`. Команда изменяет содержимое карты и предназначена для сервисной проверки записи.

## 8.4 RTC

Короткий вывод времени:

```text
time
```

Alias:

```text
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

Операция неблокирующая. После команды можно сразу выполнять `field`, `x2x` или `eth`.

## 8.5 Watchdog

```text
watchdog
watchdog feed
watchdog test reset
```

`watchdog test reset` намеренно прекращает автоматический feed. Контроллер должен аппаратно перезагрузиться. На следующем старте возможен дополнительный USB-recovery reset.

Команду выполнять только тогда, когда контролируемый reset допустим.

## 8.6 Аппаратные UART

```text
rs485
sc16is
```

`rs485` показывает владельцев встроенных UART, format и hardware errors.

`sc16is` показывает фактически обнаруженную аппаратную комплектацию внешних UART:

- два SC16IS740;
- либо SC16IS752 и каналы A/B;
- назначение PC, HMI и S1.

Файл `SC16.txt` не применяется: аппаратная схема определяется автоматически.

---

## 9. Диагностика FieldSensor S1–S4

Команды:

```text
field
field reload
field pause
field resume
```

### `field`

Показывает для каждого порта:

- роль;
- наличие transport;
- фактический baudrate и frame;
- slave address;
- функцию и диапазон регистров;
- period и timeout;
- connection/valid/request state;
- последние два регистра;
- success/failed counters;
- consecutive failures;
- UART errors;
- время последнего обновления.

### Нормальное состояние подключённого прибора

```text
connection = online
valid = yes
last_result = ok
success растёт
consecutive_failures = 0
uart_errors = 0
```

### Отключённый прибор

После пяти последовательных ошибок:

```text
connection = lost
valid = no
```

Последние корректные значения сохраняются. Использовать их можно только при `valid = yes`.

### `field reload`

Повторно читает:

```text
baud.TXT
Parity.TXT
```

Активные транзакции сначала завершаются, затем S1–S4 безопасно переинициализируются.

### `field pause` / `field resume`

Pause запрещает запуск новых запросов, но не обрывает уже начатую транзакцию.

---

## 10. Подключение внешнего Modbus RTU-прибора

Текущий baseline использует один демонстрационный прибор на каждом S1–S4:

```text
slave address = 1
function = FC03 Read Holding Registers
start register = 0
register count = 2
poll period = 300 ms
timeout = 500 ms
```

### 10.1 Физические порты

```text
S1 -> SC16IS7xx
S2 -> ATSAM3X8E UART1 / Serial1
S3 -> ATSAM3X8E UART3 / Serial3
S4 -> ATSAM3X8E UART2 / Serial2
```

### 10.2 Настройка baudrate

`baud.TXT`:

```text
4
9600
9600
1200
9600
fin
```

Порядок:

```text
S1
S2
S3
S4
```

Значения являются реальными baudrate, а не индексами таблицы.

### 10.3 Настройка parity

`Parity.TXT`:

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

### 10.4 Где изменить параметры прибора

Файл:

```text
app/field/field_sensor_service.cpp
```

Функция:

```cpp
set_default_configs()
```

Основные поля:

```cpp
g_configs[index].slave_address
g_configs[index].start_register
g_configs[index].register_count
g_configs[index].poll_period_ms
g_configs[index].timeout_ms
```

Пример другого прибора:

```cpp
g_configs[index].role = FIELD_PORT_MASTER;
g_configs[index].slave_address = 7U;
g_configs[index].start_register = 100U;
g_configs[index].register_count = 4U;
g_configs[index].poll_period_ms = 1000U;
g_configs[index].timeout_ms = 700U;
```

### 10.5 Если прибор возвращает больше двух регистров

Текущий размер:

```cpp
FIELD_SENSOR_REGISTER_COUNT = 2U
```

Находится в:

```text
app/field/field_sensor_service.hpp
```

При увеличении необходимо одновременно проверить:

1. `FIELD_SENSOR_REGISTER_COUNT`;
2. `FieldSensorPortState::registers`;
3. `set_default_configs()`;
4. обработку и декодирование данных;
5. `field_status.cpp`;
6. карту `ethernet_modbus_service.cpp`;
7. `ethernet_status.cpp`;
8. документацию Modbus TCP.

Нельзя увеличить только `register_count`, не увеличив runtime buffer.

### 10.6 Добавление нового типа прибора

Для одного простого прибора допустимо изменить baseline-конфигурацию.

Если прибор имеет:

- отдельную state machine;
- несколько последовательных запросов;
- команды записи;
- собственные диагностические поля;
- особое преобразование регистров;

лучше создать отдельный service по образцу `field_sensor_service.*`, а не усложнять общий Modbus RTU master.

`ModbusRtuMaster` должен оставаться универсальным protocol engine и не знать конкретную модель устройства.

### 10.7 Пример неблокирующего опроса

Один цикл должен выглядеть логически так:

```text
1. вызвать modbus_rtu_master_poll()
2. если активная транзакция завершилась — обработать результат
3. если наступил срок и master ready — запустить следующий запрос
4. сразу вернуть управление основной task
```

Запрещено:

```text
while (ожидание ответа)
delay(...)
busy-wait аппаратного флага
```

---

## 11. X2X: настройка и использование

Команды:

```text
x2x
x2x reload
x2x pause
x2x resume
x2x ldo SLAVE VALUE
```

### 11.1 Формат X2X.TXT

Первая строка — количество модулей. Далее — числовые ID в физическом порядке цепочки.

Пример:

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

В этом случае UART0 используется fallback echo-test.

### 11.2 Текущие стабильные ID

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

### 11.3 Диагностика X2X

```text
x2x
```

Для каждого модуля проверять:

```text
connection = online
communication_error = ok
success растёт
consecutive_failures = 0
uart_errors = 0
```

После отключения ожидается рост `failed`. После порога ошибок:

```text
connection = lost
```

После восстановления первый корректный цикл должен вернуть:

```text
connection = online
communication_error = ok
```

### 11.4 Reload

```text
x2x reload
```

Если текущий цикл ещё выполняется, применение откладывается до безопасной границы. Невалидный новый файл не должен разрушать последнюю рабочую конфигурацию.

### 11.5 Pause/resume

```text
x2x pause
x2x resume
```

Pause завершает активный цикл и только затем останавливает новые опросы.

### 11.6 Проверка LDO

```text
x2x ldo 1 255
```

Команда записывает локальный output value модуля LDO1118. Передача выполняется драйвером при следующем цикле.

Это сервисная команда, а не прикладная PLC-логика.

---

## 12. Добавление нового X2X-модуля

Новый обычный модуль требует изменений в четырёх местах:

1. стабильный ID и runtime-структура;
2. driver entry point;
3. реализация драйвера;
4. запись в каталоге;
5. добавление `.cpp` в Microchip Studio.

Служебные уровни `x2x_config`, `x2x_registry`, `x2x_service` и `x2x_status` обычно не изменяются.

### 12.1 Шаг 1 — стабильный ID

Файл:

```text
app/x2x/x2x_types.hpp
```

Пример:

```cpp
enum X2XDeviceType : uint16_t
{
    // существующие ID
    X2X_DEVICE_LTM1114 = 7U
};
```

ID 7 после публикации становится частью внешнего формата `X2X.TXT`.

### 12.2 Шаг 2 — runtime-структура

Пример:

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

Общий `X2XDeviceHeader` уже содержит:

- slave address;
- ASDU;
- connection state;
- communication result;
- exception code;
- success/failed counters;
- last update time.

Не нужно дублировать эти поля в структуре нового типа.

### 12.3 Ограничение размера

Каждый модуль создаётся в статическом слоте:

```cpp
X2X_DEVICE_SLOT_BYTES = 256U
```

`construct_device()` содержит compile-time проверку размера. Если структура больше слота, проект не соберётся.

Большие массивы, например осциллограммы, не следует помещать в каждый экземпляр. Для LCT используется отдельный общий waveform buffer.

### 12.4 Шаг 3 — объявление драйвера

Файл:

```text
app/x2x/modules/x2x_module_drivers.hpp
```

Добавить:

```cpp
X2XModulePollResult x2x_module_poll_ltm1114(
    X2XDeviceHeader* device,
    X2XModuleContext& context);
```

### 12.5 Шаг 4 — реализация драйвера

Создать:

```text
app/x2x/modules/x2x_ltm.cpp
```

В качестве образца использовать ближайший по типу файл:

```text
x2x_ldi.cpp  -> дискретные входы
x2x_lai.cpp  -> DI + float values
x2x_ldo.cpp  -> запись выходов
x2x_lct.cpp  -> многошаговый драйвер и waveform
```

### 12.6 Контекст драйвера

`X2XModuleContext` предоставляет:

```text
master
runtime
waveform
register_buffer
register_capacity
now_ms
```

Драйвер не создаёт собственный Modbus master. Вся цепочка использует общий master, а сервис вызывает драйверы последовательно.

### 12.7 Требование неблокирующей работы

Один вызов драйвера выполняет один шаг:

```text
IDLE
  -> проверить master ready
  -> запустить запрос
  -> вернуть IN_PROGRESS

WAIT_READ
  -> вызвать/проверить master
  -> если busy, вернуть IN_PROGRESS
  -> если завершено, декодировать данные
  -> mark success/failure
  -> вернуть CYCLE_COMPLETE
```

Драйвер не должен ждать ответ внутри одного вызова.

### 12.8 Упрощённый skeleton

```cpp
#include "x2x_module_drivers.hpp"
#include "x2x_module_common.hpp"

namespace
{
enum LtmState : uint8_t
{
    LTM_STATE_IDLE = 0U,
    LTM_STATE_WAIT_VALUES = 1U
};
}

X2XModulePollResult x2x_module_poll_ltm1114(
    X2XDeviceHeader* header,
    X2XModuleContext& context)
{
    X2XLtm1114& device = static_cast<X2XLtm1114&>(*header);

    modbus_rtu_master_poll(*context.master);

    switch (context.runtime->state)
    {
        case LTM_STATE_IDLE:
        {
            if (modbus_rtu_master_ready(*context.master) == 0U)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            const uint8_t started = modbus_rtu_master_start_read_holding(
                *context.master,
                device.slave_address,
                0U,
                9U,
                context.register_buffer,
                X2X_MODBUS_TRANSACTION_TIMEOUT_MS);

            if (started == 0U)
            {
                x2x_module_mark_failure(
                    device,
                    modbus_rtu_master_result(*context.master),
                    modbus_rtu_master_exception_code(*context.master));
                modbus_rtu_master_reset(*context.master);
                return X2X_MODULE_POLL_CYCLE_COMPLETE;
            }

            context.runtime->state = LTM_STATE_WAIT_VALUES;
            return X2X_MODULE_POLL_IN_PROGRESS;
        }

        case LTM_STATE_WAIT_VALUES:
        default:
        {
            if (modbus_rtu_master_busy(*context.master) != 0U)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            if (modbus_rtu_master_result(*context.master) ==
                MODBUS_RTU_RESULT_OK)
            {
                device.digital_inputs = context.register_buffer[0];
                x2x_module_decode_float_array(
                    device.tf_values,
                    X2XLtm1114::TF_COUNT,
                    context.register_buffer,
                    1U,
                    9U);

                x2x_module_mark_success(device, context.now_ms);
            }
            else
            {
                x2x_module_mark_failure(
                    device,
                    modbus_rtu_master_result(*context.master),
                    modbus_rtu_master_exception_code(*context.master));
            }

            modbus_rtu_master_reset(*context.master);
            x2x_module_runtime_reset(*context.runtime);
            return X2X_MODULE_POLL_CYCLE_COMPLETE;
        }
    }
}
```

Skeleton является примером структуры. Адреса и количество регистров должны соответствовать фактической карте нового модуля.

### 12.9 Float word order

Штатные helpers ожидают:

```text
первый Modbus-регистр  -> младшее 16-битное слово
второй Modbus-регистр -> старшее 16-битное слово
```

Использовать:

```cpp
x2x_module_registers_to_float(...)
x2x_module_decode_float_array(...)
```

Если новый модуль использует другой word order, это должно быть явно реализовано и описано в драйвере конкретного типа. Нельзя глобально менять helper и ломать совместимость существующих модулей.

### 12.10 Шаг 5 — регистрация в каталоге

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

Каталог автоматически подключает новый тип к:

- проверке `X2X.TXT`;
- статическому registry;
- циклическому service;
- общему status;
- специфическому print callback.

### 12.11 Нестандартная диагностика

Для обычного DI-модуля:

```cpp
print_digital_inputs<X2XLtm1114>
```

Для DI + float:

```cpp
print_digital_inputs_and_floats<X2XLtm1114>
```

Для особой структуры создать отдельную print-функцию в `x2x_catalog.cpp`.

Параметры console оформлять так:

```text
name = value
0: value
```

Для единообразия использовать helpers из:

```text
app/diagnostics/diagnostic_output.hpp
```

### 12.12 Добавление файла в проект

В `LCP_Basic.cppproj`:

```xml
<Compile Include="app\x2x\modules\x2x_ltm.cpp">
    <SubType>compile</SubType>
</Compile>
```

Либо добавить файл через Microchip Studio.

### 12.13 Проверка нового модуля

1. собрать Debug и Release;
2. создать `X2X.TXT` с новым ID;
3. выполнить `x2x reload`;
4. проверить правильное имя и SP/ME/TF;
5. проверить рост `success`;
6. изменить физические входы и сравнить данные;
7. отключить линию и проверить loss state;
8. восстановить линию и проверить automatic recovery;
9. выполнить `rtos` до и после длительного прогона;
10. зафиксировать register map и word order в Markdown-документации.

---

## 13. Ethernet и Modbus TCP

Оба W5500 запускают независимые server instance.

```text
protocol = Modbus TCP
port = 502
function = FC03 Read Holding Registers
map = 0..11
write functions = not supported
```

ETH1 и ETH2 публикуют одну и ту же карту FieldSensor.

### 13.1 Сетевые файлы

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

### 13.2 Формат IP-файла

```text
4
192
168
1
1
fin
```

### 13.3 Формат MAC-файла

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

### 13.4 Два одинаковых IP

Одинаковый IP допустим только при подключении ETH1 и ETH2 к физически раздельным сетям.

Если оба интерфейса подключены к одной LAN, назначить разные IP.

### 13.5 Применение настроек

```text
eth reload
```

Команда повторно читает все восемь файлов и переинициализирует оба W5500.

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

### 13.6 Holding register map

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

Чтение всех данных:

```text
FC03
start address = 0
quantity = 12
```

### 13.7 Quality register

```text
bit 0      valid
bit 1      connection_lost
bit 2      physical port present
bit 3      RTU request active
bit 4      FieldSensor service paused
bits 8..15 ModbusRtuResult
```

Значения `ModbusRtuResult`:

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

Клиент должен проверять quality. Два value-регистра могут содержать last-good data после потери связи.

### 13.8 Диагностика Ethernet

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

### 13.9 Расширение holding map

При расширении текущей карты изменять:

```text
app/ethernet/ethernet_modbus_service.hpp
app/ethernet/ethernet_modbus_service.cpp
app/diagnostics/ethernet_status.cpp
docs/ModbusTcpExample.md
docs/FirmwareGuide.md
```

Порядок:

1. определить новый размер map;
2. увеличить статический holding buffer;
3. заполнить новые адреса в `update_holding_map()`;
4. проверить диапазоны в `read_holding()`;
5. обновить compile-time `static_assert`;
6. обновить console-output;
7. описать карту в документации;
8. протестировать минимальный и максимальный адрес.

### 13.10 Добавление новой Modbus TCP function

Сначала определить, является ли функция общей protocol-возможностью или прикладной операцией.

- MBAP parsing и общая реализация function code находятся в `protocol/modbus_tcp/`;
- прикладная карта и callback находятся в `app/ethernet/`;
- W5500 HAL не должен знать Modbus-регистры.

Не следует добавлять прикладную логику непосредственно в `w5500_lite.cpp`.

---

## 14. Как читать диагностические результаты

## 14.1 Modbus RTU result

```text
idle             запрос отсутствует
busy             транзакция выполняется
ok               корректный ответ
 timeout          ответ не получен в срок
CRC error         неверный CRC
invalid response  неверный адрес, функция, длина или данные
exception         slave вернул exception
transport error   ошибка UART/TX
invalid argument  неверные параметры запуска
```

## 14.2 `request = busy` вместе с `last_result = ok`

Это нормальное состояние:

- предыдущий запрос завершился успешно;
- следующий запрос уже выполняется.

## 14.3 `consecutive_failures = 255`

Счётчик насыщается на 255 и не переполняется. Это означает длительное отсутствие корректных ответов.

## 14.4 Сохранённые значения при `valid = no`

Прошивка сохраняет last-good values. Это позволяет видеть последнее принятое значение, но прикладная логика обязана учитывать `valid` и `connection_lost`.

## 14.5 X2X exception code 2

Обычно означает `Illegal Data Address`: карта регистров в драйвере не соответствует версии firmware модуля либо выбран неверный тип модуля.

Проверить:

1. ID в `X2X.TXT`;
2. версию платы/firmware модуля;
3. start address и register count драйвера;
4. документацию register map.

## 14.6 Ethernet `link = down`

Проверить:

- кабель;
- коммутатор;
- питание W5500;
- правильный разъём ETH1/ETH2;
- `VERSIONR = 0x04`;
- socket state после подключения.

## 14.7 microSD file not found

Проверить:

- файл находится в корне;
- точный регистр имени;
- расширение `.TXT` или `.txt`;
- FAT16/FAT32;
- отсутствие дополнительного расширения, например `.txt.txt`.

## 14.8 Неожиданный reset

Выполнить:

```text
watchdog
uptime
```

Проверить reset cause и boot count.

## 14.9 Подозрение на утечку памяти

Выполнить несколько раз одинаковую последовательность:

```text
status
rtos
```

Сравнить:

- current free heap;
- minimum-ever-free heap;
- stack high-water mark.

Current free heap не должен уменьшаться после каждого одинакового цикла.

---

## 15. Добавление новой console-команды

Основной файл:

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
4. добавить краткий diagnostic output;
5. проверить неизвестную и некорректную форму команды;
6. проверить, что команда не блокирует основную task.

### Правила console-output

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

Не создавать вручную новый стиль вывода в каждом файле.

Каждая строка должна по возможности помещаться в терминал шириной около 80 символов.

---

## 16. Правила разработки

### 16.1 Неблокирующая работа

Основная task обслуживает все подсистемы. Поэтому запрещены длительные блокировки.

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

### 16.3 Разделение уровней

```text
HAL       -> регистры и аппаратные операции
board     -> разводка конкретной платы
protocol  -> универсальный протокол
service   -> конкретное назначение протокола
status    -> только диагностика и команды
```

Примеры неправильного смешения:

- W5500 HAL знает структуру FieldSensor;
- Modbus RTU master знает модель прибора;
- console напрямую меняет внутреннее состояние protocol engine;
- X2X-драйвер читает microSD самостоятельно.

### 16.4 Стиль C/C++

Настройки находятся в:

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

---

## 17. Минимальный тест после изменения прошивки

После любого функционального изменения:

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

Для FieldSensor:

1. подключить прибор;
2. проверить online/valid;
3. проверить значения;
4. отключить;
5. проверить lost/invalid;
6. восстановить и проверить recovery.

Для X2X:

1. проверить каждый добавленный тип;
2. проверить физическое изменение входов/выходов;
3. проверить exception и timeout path;
4. проверить recovery.

Для Ethernet:

1. FC03 `0..11` на ETH1;
2. FC03 `0..11` на ETH2;
3. сравнить с `field`;
4. отключить и вернуть link;
5. убедиться, что transport errors не растут.

После нагрузки повторить:

```text
rtos
```

---

## 18. Release checklist

Перед release:

1. Debug build успешен;
2. Release build успешен;
3. compiler warnings отсутствуют либо объяснены;
4. Flash и SRAM не превышают лимиты;
5. версия и stage корректны;
6. microSD canonical files проверены;
7. S1–S4 проверены;
8. ETH1/ETH2 проверены;
9. X2X проверен на фактических модулях;
10. RTC и watchdog проверены;
11. heap и stack имеют запас;
12. документация обновлена;
13. release commit/tag создан;
14. рабочая ветка объединена с `main`.

---

## 19. Связанные документы

```text
docs/AddingX2XModule.md
    отдельная пошаговая инструкция добавления X2X-типа

docs/FieldSensorExample.md
    подробности текущего S1–S4 baseline

docs/ModbusTcpExample.md
    текущая Ethernet-карта и файлы microSD

docs/X2XRuntimeControl.md
    управление X2X через console

docs/X2XBaselineTest.md
    аппаратная проверка X2X

docs/ReleaseVerification.md
    зафиксированные результаты release-кандидата

docs/CodeQualityReview.md
    архитектурные решения и результаты code review
```

При расхождении документации и кода сначала проверить текущую версию ветки и историю изменений. После изменения внешнего формата, register map или команды соответствующий Markdown-файл должен обновляться в том же commit.
