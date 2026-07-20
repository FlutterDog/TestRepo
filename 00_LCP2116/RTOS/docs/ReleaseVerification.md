# Проверка кандидата базовой прошивки Lorentz

## Статус документа

Этот файл фиксирует результаты аппаратного прогона версии:

```text
0.18.3
Stage 18D: Final code and console review
```

После прогона были выполнены только правки представления console и добавлена
команда `time`. Поэтому текущая версия `0.18.4 / Stage 18E` должна быть повторно
собрана перед release. Результаты 18D подтверждают алгоритмы и периферию, но не
заменяют компиляцию 18E.

## Подтверждено на LCP2116

### Запуск и RTOS

- прошивка компилируется, прошивается и запускает scheduler;
- USB CDC console остаётся доступной после длинного `status`;
- unknown command обрабатывается без нарушения работы;
- uptime увеличивается и после штатной работы, и после watchdog reset.

Зафиксированное состояние памяти после нагрузки:

```text
ATSAM3X8E SRAM total                  98304 bytes
assigned by linker                   51176 bytes, 52.1 %
linker-unassigned reserve            47128 bytes, 47.9 %

FreeRTOS heap total                  32768 bytes
heap used / peak used                10280 bytes, 31.4 %
heap free / minimum-ever-free        22488 bytes, 68.6 %

LCP task stack total                  8192 bytes
peak used                             1096 bytes, 13.4 %
minimum free                          7096 bytes, 86.6 %
```

Два последовательных отчёта `rtos` дали одинаковые значения heap и stack.
Признаков утечки памяти в выполненном коротком прогоне не обнаружено.

### microSD

- карта обнаружена;
- FAT32 смонтирован;
- filesystem ready;
- `Parity.TXT` прочитан успешно;
- все восемь канонических Ethernet-файлов прочитаны успешно;
- `SDTEST.TXT` присутствует.

### FieldSensor

Порт S1 подтверждён реальным Modbus RTU slave:

- 9600 8N1;
- slave address 1;
- FC03, registers 0 and 1;
- более 2200 успешных запросов;
- `valid=yes`, `connection=online`;
- значения опубликованы через Modbus TCP;
- после отдельных ошибок связь восстановилась автоматически;
- UART error counter оставался равным нулю.

### Ethernet

ETH1:

- W5500 VERSIONR = 0x04;
- link up/down определяется;
- 4550 запросов и 4550 ответов;
- exceptions = 0;
- malformed = 0;
- transport errors = 0;
- после отключения и возврата link интерфейс продолжил работать.

ETH2:

- W5500 VERSIONR = 0x04;
- link up/down определяется;
- 55 запросов и 55 ответов;
- malformed = 0;
- transport errors = 0;
- зафиксированы 4 Modbus exception response. Это не transport failure, но перед
  release желательно подтвердить, что exception были вызваны намеренно
  некорректными адресами или количеством регистров клиента.

### RTC

- внешний slow-clock crystal определяется;
- чтение даты и времени работает;
- `rtc set` запускается неблокирующе;
- дата и время успешно изменены на `2026-07-20 20:30:00`;
- последующее чтение показало продолжающийся ход RTC;
- другие console-команды выполнялись после операции.

### Watchdog

- hardware watchdog включён;
- команда `watchdog test reset` вызвала ожидаемый reset;
- на следующем старте выполнен предусмотренный USB-recovery software reset;
- boot counter увеличился;
- console восстановилась.

### SC16IS

Автоопределена комплектация с двумя одноканальными SC16IS740:

```text
PC  -> UART1_CS/A
HMI -> UART2_CS/A
S1  -> UART3_CS/A
```

## Не подтверждено текущим прогоном

### Сборка

В присланном журнале отсутствует полный Build Output Microchip Studio. Поэтому
пока не зафиксированы:

- отдельный результат Debug build;
- отдельный результат Release build;
- количество compiler warnings в каждой конфигурации;
- размер Flash для Debug и Release;
- окончательные linker totals после изменений Stage 18E.

### microSD serial configuration

`baud.TXT` вернул `file not found`. Поэтому чтение фактических скоростей из файла
не подтверждено. Порты работали с безопасным значением 9600 baud. До release
нужно положить канонический `baud.TXT`, выполнить `field reload` и убедиться, что
значения каждого S1..S4 совпадают с файлом.

### FieldSensor

- S2 не имел подключённого slave;
- S3 не имел подключённого slave;
- S4 не имел подключённого slave;
- одновременная связь по всем четырём портам не подтверждена.

Timeout и `connection=lost` на этих линиях соответствуют отсутствию устройств и
не считаются дефектом firmware.

### X2X

`X2X.TXT` успешно создал один LCT1114_2, но модуль в данном прогоне не отвечал:

```text
success=0
communication_error=timeout
connection=lost
```

Следовательно, конфигурация, registry и timeout-path подтверждены, а успешный
обмен X2X именно для Stage 18D не подтверждён. Ранее пройденные тесты других
версий остаются полезной регрессией, но не заменяют финальную проверку текущего
кандидата с реально подключённым модулем.

### Diagnostic echo

Не подтверждены длинные кадры:

- PC echo через SC16IS;
- HMI echo через SC16IS;
- fallback echo физической X2X-линии при пустом `X2X.TXT`.

Эти echo-сервисы остаются в release намеренно, поскольку продукт называется
`LCP Basic Diagnostic Firmware` и не содержит реального PC/HMI protocol. При
добавлении настоящего владельца соответствующего UART echo необходимо отключить
до инициализации нового protocol service. Echo для S1..S4 удалён и в runtime не
участвует.

### Длительный прогон

Имеющийся журнал охватывает примерно 12 минут до watchdog-теста. Для release
желателен совместный прогон X2X + FieldSensor + ETH1 + ETH2 + USB console не
менее 1 часа, если стенд позволяет.

## Сопоставление наблюдений с подсистемами

| Наблюдение | Подсистема | Оценка |
|---|---|---|
| `baud.TXT file not found` | `field_serial_config` / содержимое SD | файл отсутствует; не runtime defect |
| S2..S4 timeout | `field_sensor_service` | ожидаемо без slave |
| LCT1114_2 timeout | `x2x_service` / физическая X2X-линия | успешный путь не проверен |
| ETH2 exception count = 4 | `modbus_tcp_server` | требуется подтвердить запросы клиента |
| странные пробелы внутри слов | PuTTY soft wrap длинных строк | сокращено в Stage 18E |
| неудобно смотреть текущее RTC | `rtc_status` / console UX | добавлены `time` и `rtc time` |

## Решение по release

Release version и merge разрешаются только после:

1. успешной компиляции Stage 18E в Debug и Release;
2. отсутствия новых warnings либо документированного объяснения каждого warning;
3. фиксации Flash/RAM totals из linker output;
4. проверки `time`, `help`, `status`, `field`, `x2x`, `eth` после console-правок;
5. явного решения, допустимы ли непроверенные S2..S4 и X2X в release scope;
6. явного подтверждения владельца репозитория на merge.
