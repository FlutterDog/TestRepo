# Ревизия качества базовой прошивки Lorentz

## Область проверки

Ревизия относится к собственному коду `00_LCP2116/RTOS`. Сторонние FreeRTOS, CMSIS, ASF и USB-компоненты не перерабатывались.

Проверены:

- lifecycle единственной прикладной задачи FreeRTOS;
- циклы, state machine, timeout и wrap-around `millis()`;
- X2X config, catalog, registry, service и драйверы модулей;
- Modbus RTU master и минимальный slave-пример;
- FieldSensor S1–S4 и безопасное применение serial-конфигурации;
- microSD и числовые TXT-файлы;
- два W5500, потоковый Modbus TCP server и карта FieldSensor;
- диагностические echo-режимы и владение UART;
- статические буферы, границы массивов и отсутствие прикладной динамической памяти;
- service console, человекочитаемые метрики и Doxygen;
- состав и параметры Debug/Release проекта Microchip Studio.

## Изменения первого прохода

### Один числовой TXT-парсер

Serial-конфигурация, Ethernet-конфигурация и диагностический слой больше не содержат собственные варианты одинакового parser. Один общий модуль:

```text
app/diagnostics/sd_config.cpp
app/diagnostics/sd_config.hpp
```

поддерживает `int16_t`, `uint32_t`, UTF-8 BOM, пустые строки, комментарии `//` и `#`, необязательный `fin`, строгий count, лишние/недостающие значения и числовое переполнение без динамической памяти.

`field_serial_config` и `ethernet_network_config` отвечают только за предметную проверку и применение.

### Целостная передача Modbus TCP ADU

W5500 HAL использует контракт:

```text
полный ADU помещается -> передать;
полный ADU не помещается -> ничего не передавать и повторить позже.
```

`ModbusTcpServer::tx_buffer` сохраняется до подтверждения полной постановки кадра в W5500 TX memory. Частичный Modbus TCP response не теряется и не усекается.

### Удаление старого Ethernet echo

Неиспользуемые TCP echo buffers и wrapper удалены из `w5500_lite`. Ethernet baseline имеет одну роль: два независимых Modbus TCP server.

### Формализованная карта качества

Биты FieldSensor quality вынесены в именованный enum. `static_assert` связывает четыре физических порта с 12 регистрами опубликованной карты.

## Изменения второго прохода

### Удалён busy-wait RTC

Ранее `rtc set` мог до 1,5 секунды ждать завершения update внутри обработчика команды. Поскольку console работает в единственной LCP task, на это время останавливались X2X, FieldSensor, Ethernet и остальные poll-функции.

Теперь команда только запускает обновление. ACKUPD, запись TIMR/CALR, проверка результата и timeout обслуживаются через `rtc_status_poll()`. Удалён также ежесекундный RTC cache, который обновлялся, но нигде не использовался.

### Ограничены все состояния ожидания TX

Проверены protocol и diagnostic state machine:

- Modbus RTU master уже имел ограниченный TX/response timeout;
- минимальный Modbus RTU slave получил `MODBUS_RTU_SLAVE_TX_TIMEOUT_MS`;
- fallback echo UART0/X2X получил ограниченный TX timeout;
- echo PC/HMI SC16IS получил ограниченный TX timeout.

Неисправный transport больше не может навсегда удерживать `WAIT_TX` или незавершённый echo-frame.

### Исправлена частичная передача echo

Обе echo-реализации раньше могли потерять хвост или повторить начало кадра, если UART/FIFO принимал только часть buffer.

Теперь хранится `tx_offset`, и каждый следующий `poll()` продолжает с первого неотправленного байта. Кадр очищается только после полной постановки либо по timeout.

### Удалён мёртвый echo S1–S4

S1–S4 ранее кратковременно запускались как echo, после чего в том же `setup()` перенастраивались под FieldSensor до первого `poll()`. Через console этот echo включить было нельзя.

Удалены:

- временный echo S1 в SC16IS service;
- временный echo S2–S4 во встроенном RS-485 service;
- связанные enable/disable API;
- общий bulk-initializer всех SC16IS-каналов.

Теперь владельцы однозначны:

```text
UART0/X2X -> X2X master или fallback echo при пустом X2X.TXT
S1..S4    -> FieldSensor master
PC/HMI    -> diagnostic SC16IS echo
```

Одновременно удалено 512 байт статических echo-buffers.

### SC16IS probe выполняется один раз

SC16IS являются стационарными компонентами платы и не поддерживают hot-plug. `lcp_sc16is_init_pins()` и `lcp_sc16is_probe()` сделаны идемпотентными. Echo, FieldSensor и serial reload используют одну сохранённую логическую карту PC/HMI/S1.

### Ужесточены Modbus-границы

Modbus RTU master и slave принимают обычные slave address только `1..247`. Broadcast 0 намеренно не поддерживается baseline API.

Для RX/TX capacities добавлены compile-time проверки. Внутренний parser команды `x2x ldo` проверяет переполнение до умножения, а не после возможного wrap-around.

### Централизован fail-stop RTOS

Три одинаковых фатальных цикла сведены к одной документированной `fatal_stop()`.

`xTaskCreate()` теперь проверяется не только через `configASSERT`, но и явным условием, работающим в Release. Обычный бесконечный цикл LCP task остаётся штатным и на каждом проходе вызывает `vTaskDelay(1)`.

## Service console

Создан единый formatter `diagnostic_output.hpp`:

- крупные banner;
- заголовки subsystem и group;
- UART frame text;
- проценты с одной десятичной цифрой без float/printf;
- длительность `D d HH:MM:SS`.

Команда `help` разделена на группы. Добавлены aliases `?` и `ver`.

`status` теперь состоит из визуально отделённых подсистем, а не из непрерывной простыни.

`uptime` показывает одновременно:

```text
uptime_ms=<raw counter>
uptime_human=<days> d <HH:MM:SS>
```

`rtos` показывает абсолютные значения и проценты:

- SRAM assigned/unassigned;
- heap current used/free;
- heap peak used/minimum ever free;
- task stack peak used/minimum free.

Отдельно указано, что FreeRTOS heap является массивом внутри `.bss` и не должен повторно прибавляться к общему расходу SRAM.

FieldSensor и X2X показывают `last_update_ms`, age в миллисекундах и человекочитаемый age. Ethernet report расшифровывает quality каждого S1–S4 вместо вывода одного списка из 12 чисел.

## Doxygen как карта расширения

Комментарии описывают не только назначение API, но и точку изменения:

### Новый X2X-модуль

1. runtime-структура в `x2x_types.hpp`;
2. отдельный driver entry point;
3. descriptor `construct/poll/print` в `x2x_catalog.cpp`.

Service, registry и console не требуют нового switch.

### Новый внешний прибор

- device address, FC03 start/count, period и timeout меняются в `set_default_configs()` FieldSensor service;
- baud/parity находятся в `field_serial_config.*`;
- физическая привязка UART находится в `board/lcp_field_ports.*`;
- другая модель или другой протокол оформляются отдельным прикладным service поверх `ModbusRtuMaster`.

При увеличении количества регистров одновременно проверяются runtime buffer, Ethernet map и диагностика.

### Новые Modbus TCP-регистры

Меняются holding count и `update_holding_map()` в `ethernet_modbus_service`. Потоковый parser и W5500 HAL не меняются.

### Новая Modbus TCP-функция

Сначала добавляется callback в `protocol/modbus_tcp`, затем подключается в прикладном Ethernet service. FieldSensor/X2X-зависимости не помещаются в W5500 HAL.

### Новая console-команда

1. handler добавляется в локальный dispatcher либо профильный `*_status_handle_command()`;
2. help entry добавляется в соответствующую группу;
3. длительная операция хранит state и завершается через `poll()`, без busy-wait.

## Что намеренно не объединялось

### Драйверы X2X

`x2x_lai.cpp`, `x2x_ldi.cpp`, `x2x_ldo.cpp` и `x2x_lct.cpp` остаются отдельными. У них разные карты и state machine. Общая механика уже находится в `x2x_module_common`, catalog и registry.

### LCT1114 и LCT1114_2

Они используют общий LCT driver, но разные точные runtime-структуры. Это сохраняет соответствие аппаратным картам и не скрывает смещения полей.

### FieldSensor config и runtime

`FieldSensorConfig` нужен для staged apply. `FieldSensorPortState` хранит master, buffer, quality, counters и timing. Объединение ухудшило бы безопасный reload.

### RTOS-задачи

Не создавались отдельные задачи для X2X, четырёх FieldSensor и двух Ethernet-портов. Все services неблокирующие. Одна task сохраняет прозрачное владение UART/SPI и исключает лишние mutex, queues и stacks.

## Память

Прикладной код не использует `new`, `delete`, `malloc` или `free`.

Основные статические области:

- FreeRTOS heap_4;
- четыре `ModbusRtuMaster` FieldSensor;
- один `ModbusRtuMaster` X2X;
- X2X waveform `3 × 400` samples;
- два `ModbusTcpServer`;
- X2X static registry;
- PC/HMI echo buffers.

Удаление временного echo S1–S4 уменьшило статическую память на 512 байт. Фактические `.bss`, heap и stack metrics должны быть повторно сняты после ARM-сборки текущей версии.

## Финальный аппаратный прогон перед релизом

Собрать Debug и Release, затем проверить:

```text
help
?
version
ver
status
uptime
rtos
x2x
field
eth
rs485
sc16is
sd
battery
rtc
watchdog
```

Отдельные сценарии:

1. `rtc set YYYY-MM-DD HH:MM:SS` запускается без остановки X2X/FieldSensor/Ethernet; затем `rtc` показывает completion.
2. ETH1 и ETH2 обрабатывают повторные FC03 без transport errors и потерянных ответов.
3. `baud.TXT` и `Parity.TXT` проверяются с комментариями и `fin`; отсутствие одного файла корректно оставляет прежний/default параметр.
4. X2X проверяется с фактически подключённым модулем; timeout отключённого LCT не считается успешным тестом.
5. PC/HMI echo, если доступен стенд, проверяется кадром длиннее одного FIFO chunk.
6. При пустом X2X.TXT проверяется fallback echo UART0; при наличии модулей UART0 принадлежит только X2X master.
7. После длинного `status` повторно снимаются heap minimum и task stack high-water mark.
8. Проверяется совместная работа X2X, FieldSensor, Ethernet и USB console.

До успешной ARM-сборки и этого прогона текущая версия считается статически проверенной, но не готовой к release/merge.
