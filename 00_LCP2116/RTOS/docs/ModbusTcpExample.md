# Пример Modbus TCP на ETH1 и ETH2

## Назначение

Базовая прошивка Lorentz запускает два независимых Modbus TCP server на двух W5500 контроллера LCP2116. Оба интерфейса публикуют одну и ту же демонстрационную карту данных `FieldSensor`.

```text
protocol: Modbus TCP
server port: 502
supported function: 0x03 Read Holding Registers
holding registers: 0..11
write functions: not supported
```

Готовые примеры штатных файлов находятся в:

```text
00_LCP2116/RTOS/sd_card/
```

## Канонические сетевые файлы microSD

Имена файлов являются частью существующего формата проектов. Альтернативные имена и псевдонимы не поддерживаются.

### ETH1

```text
MAC.txt
IP.txt
SUBNET.txt
GATE.txt
```

### ETH2

```text
MAC2.txt
IP2.txt
SUBNET2.txt
GATE2.txt
```

IP, SUBNET и GATE содержат четыре октета:

```text
4
192
168
1
1
fin
```

MAC содержит шесть байтов в десятичном виде:

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

Комментарии после `//` допускаются. После чтения указанного количества значений завершающая строка `fin` игнорируется.

Каждый из четырёх параметров интерфейса загружается независимо. При ошибке конкретного файла только соответствующий параметр сохраняет внутреннее значение по умолчанию.

## Значения присланного комплекта

```text
ETH1
MAC:     100.200.249.154.182.220
IP:      192.168.1.1
Subnet:  255.255.255.0
Gateway: 0.0.0.0

ETH2
MAC:     222.173.190.239.254.238
IP:      192.168.1.1
Subnet:  255.255.255.0
Gateway: 0.0.0.0
```

Одинаковый IP на ETH1 и ETH2 допустим, когда интерфейсы подключены к раздельным физическим сетям. При подключении обоих портов к одной сети IP-адреса должны различаться.

## Карта holding registers

Оба Ethernet-интерфейса предоставляют одинаковую read-only карту:

```text
0   FieldSensor S1 register 0
1   FieldSensor S1 register 1
2   FieldSensor S1 quality

3   FieldSensor S2 register 0
4   FieldSensor S2 register 1
5   FieldSensor S2 quality

6   FieldSensor S3 register 0
7   FieldSensor S3 register 1
8   FieldSensor S3 quality

9   FieldSensor S4 register 0
10  FieldSensor S4 register 1
11  FieldSensor S4 quality
```

Последние корректные значения сохраняются при потере RS-485 связи.

## Регистр quality

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

## Архитектура

```text
hal/w5500_lite.*
    raw W5500 socket 0, TCP receive/send, link and socket state

protocol/modbus_tcp/modbus_tcp_server.*
    MBAP framing, stream buffering, FC03 and exception responses

app/ethernet/ethernet_network_config.*
    чтение канонических MAC/IP/SUBNET/GATE файлов microSD

app/ethernet/ethernet_modbus_service.*
    два server instance и карта FieldSensor
```

Каждый W5500 имеет отдельный `ModbusTcpServer`, RX/TX-буферы и счётчики. Дополнительные FreeRTOS-задачи не создаются.

## Service console

```text
eth
eth reload
```

`eth` показывает:

- фактически применённые MAC, IP, subnet и gateway;
- каноническое имя и результат чтения каждого TXT-файла;
- VERSIONR W5500;
- link up/down;
- socket status;
- количество запросов, ответов, exception и malformed frames;
- текущие 12 holding registers.

`eth reload` повторно читает файлы и аппаратно переинициализирует оба W5500.

## Проверка

1. Скопировать канонические файлы в корень microSD.
2. Подключить ETH1 или ETH2 к компьютеру или коммутатору.
3. Настроить компьютер в соответствующей подсети.
4. Выполнить `eth` и проверить `VERSIONR=0x04`, `init=ok`, `link=up`, MAC и IP.
5. Подключиться Modbus TCP client к порту 502.
6. Выполнить FC03, start address 0, quantity 12.
7. Проверить рост `requests` и `responses` в команде `eth`.
8. Сравнить регистры 0..11 с командой `field`.
9. Повторить тест на втором Ethernet-интерфейсе.

При запросе за пределами 0..11 server возвращает exception `0x02 Illegal Data Address`. Неподдерживаемая функция возвращает `0x01 Illegal Function`.
