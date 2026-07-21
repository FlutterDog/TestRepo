# Ревизия качества базовой прошивки Lorentz

## Область проверки

Ревизия относится к собственному коду `00_LCP2116/RTOS`. Сторонние FreeRTOS,
CMSIS, ASF и USB-компоненты не перерабатывались.

Проверены:

- lifecycle одной прикладной задачи FreeRTOS;
- X2X config, catalog, registry, service и драйверы модулей;
- Modbus RTU master и минимальный slave-пример;
- FieldSensor S1–S4 и staged reload serial-конфигурации;
- общий доступ к microSD и числовые TXT-файлы;
- два W5500, потоковый Modbus TCP server и карта FieldSensor;
- статические буферы, границы массивов и отсутствие dynamic allocation;
- диагностическая console, RTC, watchdog и echo state machines;
- публичные заголовки и Doxygen;
- состав `LCP_Basic.cppproj` для Debug и Release.

Фактические аппаратные результаты и непроверенные функции вынесены отдельно:

```text
docs/ReleaseVerification.md
```

## Архитектурный результат

Принятая структура остаётся без дополнительного универсального framework:

```text
app/       application services and runtime state
board/     LCP2116 hardware mapping
hal/       ATSAM3X8E, SC16IS7xx and W5500 access
protocol/  Modbus RTU and Modbus TCP engines
platform/  common Serial/SPI/GPIO/time facade
```

Одна FreeRTOS task последовательно вызывает неблокирующие `poll()` функции. Это
упрощает владение UART/SPI и не требует mutex, queue и дополнительных стеков.
Цена решения — любая новая длительная операция обязана быть state machine, а не
ожиданием внутри command handler или service poll.

## Выполненные изменения

### Общий числовой TXT parser

Serial- и Ethernet-конфигурация используют один строгий parser:

```text
app/diagnostics/sd_config.cpp
app/diagnostics/sd_config.hpp
```

Поддерживаются UTF-8 BOM, комментарии, пустые строки, необязательный `fin`,
строгий count, проверка лишних/недостающих значений и numeric overflow.
Динамическая память не используется.

### Modbus TCP

W5500 transport передаёт только целый Modbus TCP ADU. Если TX memory временно
недостаточно, сформированный ответ сохраняется и повторяется на следующем poll.
Частичный ADU не отправляется.

### Modbus RTU

- обычные slave address ограничены диапазоном 1..247;
- размеры master/slave buffers контролируются через `static_assert`;
- master и reusable slave имеют bounded timeout;
- reusable slave не может навсегда остаться в WAIT_TX.

### RTC

`rtc set` больше не содержит busy-wait. Update выполняется через HAL state
machine и `rtc_status_poll()`. Для повседневного чтения добавлены:

```text
time
rtc time
```

Полная команда `rtc` остаётся диагностическим отчётом с raw registers.

### Echo

- S1–S4 echo удалён как мёртвый путь; эти порты сразу принадлежат FieldSensor;
- UART0 fallback echo используется только при отсутствии активной X2X chain;
- PC/HMI SC16IS echo остаётся аппаратной диагностикой baseline firmware;
- partial TX продолжается через `tx_offset`;
- TX state имеет bounded timeout;
- SC16IS probe выполняется один раз и кэширует hardware layout.

### FreeRTOS startup

Результат `xTaskCreate()` проверяется явно и в Release. Невосстановимые ошибки
scheduler, malloc и stack overflow сведены к одному документированному
`fatal_stop()`.

### Console

Отчёты разделены на sections/groups. Строки, которые в PuTTY переносились внутри
слов, разбиты по логическим полям. Help печатает command и description на разных
строках, поэтому не зависит от ширины колонок terminal.

RTOS report показывает:

- raw bytes;
- percent assigned/free;
- current and peak heap use;
- task stack high-water mark;
- raw and human-readable uptime.

### Формат исходников

В `00_LCP2116/RTOS` добавлены:

```text
.clang-format
.editorconfig
```

Для собственного C/C++ кода зафиксированы:

- 4 spaces, tabs disabled;
- Allman braces;
- spaces around assignment and binary operators;
- explicit line limit;
- final newline and no trailing whitespace.

Atmel Studio не обязан автоматически запускать clang-format, но файлы задают
однозначный contract для ручного Format Document и внешнего formatter.

## Что намеренно не объединялось

### X2X drivers

`x2x_lai.cpp`, `x2x_ldi.cpp`, `x2x_ldo.cpp` и `x2x_lct.cpp` остаются отдельными.
У них разные register maps и state machines. Общая механика уже вынесена в:

```text
x2x_module_common.*
x2x_catalog.*
x2x_registry.*
```

Дополнительный generic profile engine ухудшил бы читаемость и hardware review.

### LCT1114 и LCT1114_2

Общий алгоритм находится в одном LCT driver, но структуры данных двух версий
остаются отдельными и точно соответствуют аппаратным register maps.

### FieldSensor config и runtime

`FieldSensorConfig` и `FieldSensorPortState` не объединены. Config необходим для
staged reload, runtime содержит master, counters, quality и timers.

### RTOS tasks

Не добавлены отдельные tasks для X2X, каждого FieldSensor или каждого Ethernet.
Текущий объём и measured stack/heap не требуют этого усложнения.

## Память

В application code не используется `new`, `delete`, `malloc` или `free`.
Placement new в X2X catalog создаёт объект внутри заранее выделенного static
slot и не обращается к heap.

Аппаратный отчёт Stage 18D показал:

```text
SRAM assigned by linker      51176 / 98304 bytes = 52.1 %
FreeRTOS heap peak used      10280 / 32768 bytes = 31.4 %
LCP task stack peak used      1096 / 8192 bytes  = 13.4 %
```

Flash usage берётся только из Microchip Studio linker output и пока не зафиксирован
для Stage 18E.

## Extension points

### Новый X2X module

1. Добавить runtime structure в `x2x_types.hpp`.
2. Реализовать отдельный driver entry point.
3. Добавить descriptor в `x2x_catalog.cpp`.

Service, registry и common diagnostics нового switch не требуют.

### Новый внешний sensor/device

Transport остаётся в `board/lcp_field_ports.*`, protocol transaction — в
`ModbusRtuMaster`, а device-specific schedule/decode — в application service.
Не следует помещать model-specific registers в UART HAL.

### Новые Modbus TCP registers

Расширяется holding map в `ethernet_modbus_service`. Streaming parser и W5500 HAL
не зависят от смысла регистров.

### Новая console command

1. Добавить command handler в соответствующий subsystem.
2. Добавить help entry.
3. Не ожидать hardware event внутри handler.
4. Делить длинный output по логическим полям до 80 columns.

## Условия release

До release и merge необходимо:

1. собрать Stage 18E отдельно в Debug и Release;
2. сохранить warning count и linker Flash/RAM totals;
3. проверить `time`, `help`, `status`, `field`, `x2x`, `eth`, `rtos`;
4. принять явно перечисленные непроверенные hardware функции либо проверить их;
5. получить явное разрешение владельца repository на merge.
