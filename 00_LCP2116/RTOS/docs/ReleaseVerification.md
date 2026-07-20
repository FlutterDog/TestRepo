# Проверка release-кандидата базовой прошивки Lorentz

## Кандидат

```text
0.18.6
Stage 18G: Final index spacing
```

Функциональная часть Stage 18E была собрана в Release и проверена на LCP2116. Последующие Stage 18F/18G изменяли только оформление USB service console:

```text
name = value
0: value
```

Финальный формат визуально подтверждён пользователем.

## Подтверждено

### Сборка Release

```text
Build succeeded
0 failed
Flash/text = 126752 bytes, 24.2%
BSS = 46248 bytes, 47.0%
```

Сообщение Atmel Studio о приблизительной оценке memory usage является предупреждением инструмента, а не compiler warning исходного кода.

### RTOS и память

```text
ATSAM3X8E SRAM assigned = 51176 / 98304 bytes, 52.1%
FreeRTOS heap peak used = 10280 / 32768 bytes, 31.4%
LCP task stack peak used около 13%
```

Повторные отчёты не показали роста текущего использования heap или stack.

### microSD

Подтверждены:

- FAT32;
- `baud.TXT`;
- `Parity.TXT`;
- `MAC.txt`, `IP.txt`, `SUBNET.txt`, `GATE.txt`;
- `MAC2.txt`, `IP2.txt`, `SUBNET2.txt`, `GATE2.txt`;
- применение 1200 baud на S3.

### FieldSensor

Подтверждены реальные Modbus RTU-транзакции на S1, S2, S3 и S4:

- FC03;
- slave address 1;
- registers 0 and 1;
- потеря связи;
- `valid = no` после порога ошибок;
- сохранение last-good values;
- автоматическое восстановление;
- отсутствие UART errors.

### Modbus TCP

Подтверждены:

- ETH1 и ETH2;
- чтение FC03;
- публикация всех S1–S4 в holding registers 0..11;
- работа quality-регистров;
- восстановление ETH1 после link disconnect/reconnect;
- отсутствие malformed frames и transport errors;
- длительный обмен ETH1 более 14000 запросов без потери ответов.

### X2X

Подтверждены:

- чтение `X2X.TXT`;
- работа нескольких типов модулей;
- LCT1114_2;
- рост `success`;
- изменение `digital_inputs`;
- Modbus exception path;
- timeout/lost state;
- автоматическое восстановление connection;
- отсутствие UART errors.

### RTC, watchdog и console

Подтверждены:

- команда `time`;
- полный `rtc` report;
- неблокирующий `rtc set`;
- hardware watchdog reset;
- USB recovery после watchdog;
- сгруппированный `help`;
- разделённый `status`;
- human-readable uptime;
- проценты SRAM/heap/stack;
- финальный spacing `name = value` и `0: value`.

## Документация

Основной эксплуатационный и разработческий гайд:

```text
docs/FirmwareGuide.md
```

Он содержит:

- сборку и запуск;
- microSD;
- все команды console;
- диагностику;
- подключение FieldSensor;
- изменение Modbus RTU-прибора;
- настройку Ethernet;
- Modbus TCP map;
- добавление X2X-модуля;
- пример неблокирующего драйвера;
- release checklist.

## Решение

Технических блокеров для первого release baseline не зафиксировано.

Перед merge остаются административные действия:

1. присвоить release version;
2. обновить version/stage в исходнике;
3. создать release commit/tag;
4. объединить PR с `main` по явному подтверждению владельца репозитория.
