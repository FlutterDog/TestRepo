# Проверка кандидата базовой прошивки Lorentz

## Статус документа

Проверенный кандидат:

```text
0.18.4
Stage 18E: Final console polish
```

Версия собрана в Release-конфигурации, прошита и проверена на LCP2116.
Документ отделяет подтверждённые функции от диагностических возможностей,
которые не являются блокерами release.

## Сборка Release

Из приложенного полного Build Output:

```text
compiler flags: -DNDEBUG -Os -Wall
build result:   1 succeeded, 0 failed
compiler errors:   0
compiler warnings: 0
```

Atmel Studio вывел только собственное информационное предупреждение:

```text
Warning: Memory Usage estimation may not be accurate if there are sections
other than .text sections in ELF file
```

Это не warning компилятора исходного кода и не указывает на дефект firmware.

Размеры ELF:

```text
text = 126752 bytes
data =      0 bytes
bss  =  46248 bytes

Program Memory Usage = 126752 bytes = 24.2 %
Data Memory Usage    =  46248 bytes = 47.0 %
```

Runtime-отчёт использует более строгий linker-based расчёт SRAM, включающий
зарезервированные linker-секции:

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

Повторные отчёты `rtos` дали стабильные heap и stack значения. Признаков утечки
памяти в выполненных прогонах не обнаружено.

## Подтверждено на LCP2116

### Запуск, RTOS и console

- scheduler запускается;
- USB CDC console остаётся доступной после длинных отчётов;
- `help`, `version`, `status`, `uptime`, `rtos`, `time` и профильные команды
  выполняются;
- неизвестная команда не нарушает работу;
- новые короткие строки console не разрываются PuTTY внутри слов;
- uptime продолжает увеличиваться после штатной работы и watchdog reset.

### microSD и конфигурация

- карта обнаружена;
- FAT32 смонтирован;
- filesystem ready;
- `baud.TXT` прочитан успешно;
- `Parity.TXT` прочитан успешно;
- S3 применил 1200 baud из файла;
- все восемь канонических Ethernet-файлов прочитаны успешно;
- альтернативные имена файлов не используются.

### FieldSensor S1-S4

Один реальный Modbus RTU slave последовательно проверен на каждом физическом
порту:

```text
S1: 9600 8N1
S2: 9600 8N1
S3: 1200 8N1
S4: 9600 8N1
```

Для каждого порта подтверждены:

- slave address 1;
- FC03, registers 0 and 1;
- успешные запросы и корректные значения;
- `valid=yes`, `connection=online` при подключённом приборе;
- `connection=lost`, `valid=no` после отключения;
- сохранение last-good values;
- автоматическое восстановление после возврата прибора;
- отсутствие UART errors.

### Modbus TCP ETH1 и ETH2

Подтверждены оба W5500 и оба независимых TCP server на порту 502.

Оба интерфейса публикуют общую карту всех FieldSensor-портов:

```text
0..2   S1: value0, value1, quality
3..5   S2: value0, value1, quality
6..8   S3: value0, value1, quality
9..11  S4: value0, value1, quality
```

Внешним Modbus TCP-клиентом подтверждено чтение данных всех четырёх портов.
ETH1 и ETH2 проверены отдельно.

Длительный счётчик ETH1:

```text
requests=14428
responses=14428
exceptions=0
malformed=0
transport_errors=0
```

Повторные команды `eth` выводят устойчивую разметку без случайного разрыва
метки `IP`. Link disconnect/reconnect восстанавливается без reboot.

### X2X

Подтверждена успешная работа нескольких X2X-модулей. В приложенном финальном
фрагменте LCT1114_2 показал:

```text
connection=online
communication_error=ok
exception_code=0
success=21
consecutive_failures=0
uart_errors=0
```

Также подтверждено восстановление связи: после периода exception/timeout модуль
самостоятельно вернулся в online без перезагрузки LCP. Изменение digital input
отразилось в диагностике.

Конфигурация `X2X.TXT`, catalog, registry, polling, failure path и recovery path
подтверждены.

### RTC

- внешний slow-clock crystal определяется;
- `time` и `rtc time` выводят текущее время кратко;
- полный `rtc` report работает;
- `rtc set` запускается неблокирующе;
- дата и время устанавливаются;
- другие сервисы продолжают работать во время update sequence.

### Watchdog

- hardware watchdog включён;
- `watchdog test reset` вызывает ожидаемый reset;
- после reset выполняется предусмотренное USB recovery;
- boot counter увеличивается;
- console восстанавливается.

### SC16IS

Автоопределена комплектация с двумя одноканальными SC16IS740:

```text
PC  -> UART1_CS/A
HMI -> UART2_CS/A
S1  -> UART3_CS/A
```

## Диагностические функции, не блокирующие release

Не выполнялся отдельный формальный тест длинных echo-кадров:

- PC echo через SC16IS;
- HMI echo через SC16IS;
- fallback echo X2X UART при пустом `X2X.TXT`.

Эти сервисы не участвуют в S1-S4 FieldSensor и не владеют UART при активном X2X.
Они оставлены как diagnostic fallback в `LCP Basic Diagnostic Firmware`.
При появлении реального PC/HMI protocol соответствующий echo должен быть
отключён до инициализации protocol service.

## Итоговая оценка

Подтверждены основные функции release scope:

- FreeRTOS baseline;
- microSD configuration;
- X2X configuration, polling и recovery;
- FieldSensor на S1-S4;
- публикация S1-S4 через Modbus TCP;
- ETH1 и ETH2;
- RTC;
- watchdog;
- USB service console;
- достаточные резервы Flash, SRAM, FreeRTOS heap и task stack.

Технических runtime-блокеров release по предоставленным результатам не выявлено.

## Оставшееся административное условие

Приложенный build-log относится к Release (`-DNDEBUG -Os`). Отдельный Debug
Build Output не приложен. Это не блокирует выпуск Release-бинарника, но для
полного выполнения внутреннего чек-листа Debug build следует либо подтвердить,
либо явно исключить из release criteria.

Release version и merge выполняются только после явного подтверждения владельца
репозитория.
