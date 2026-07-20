# Проверка кандидата базовой прошивки Lorentz

## Статус документа

Аппаратный и интеграционный прогон подтвердил основной release scope версии
`0.18.4 / Stage 18E`. После этого были выполнены только изменения визуального
формата service console.

Текущий кандидат:

```text
0.18.6
Stage 18G: Final index spacing
```

Для именованных параметров используется формат:

```text
name = value
name = value, next = value
```

Для индексированных массивов используется отдельный формат:

```text
0: value, 1: value
```

Перед двоеточием пробел не ставится, после двоеточия ставится один пробел.
Двоеточия внутри MAC-адресов и времени не изменяются.

## Подтверждено на LCP2116

- scheduler, USB CDC console, RTC и watchdog reset/recovery;
- стабильные SRAM, FreeRTOS heap и LCP task-stack;
- microSD/FAT32 и канонические serial/Ethernet configuration files;
- FieldSensor communication и recovery на S1, S2, S3 и S4;
- скорость S3 = 1200 baud из `baud.TXT`;
- публикация всех четырёх портов через Modbus TCP registers 0..11;
- доступ FC03 через ETH1 и ETH2;
- восстановление ETH1 после disconnect/reconnect;
- успешный X2X-обмен с несколькими модулями, включая LCT1114_2;
- обработка X2X exception и автоматическое recovery;
- SC16IS hardware-layout auto-detection.

Release build evidence для 0.18.4:

```text
Build succeeded
0 failed
Flash/text: 126752 bytes, 24.2%
BSS: 46248 bytes, 47.0%
```

Atmel Studio memory-estimation message является предупреждением инструмента,
а не compiler warning в коде проекта.

## Перед release

1. собрать `0.18.6 / Stage 18G` в Release;
2. выполнить `version` и `x2x`;
3. визуально подтвердить формат:

```text
0: 0.00, 1: 0.00, 2: 0.00, 3: 0.00
```

4. получить явное подтверждение владельца репозитория на release versioning и merge.
