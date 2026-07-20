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

Готовые примеры файлов находятся в:

```text
00_LCP2116/RTOS/sd_card/
```

## Сетевые файлы microSD

Каждый файл использует старый числовой формат: первая строка содержит количество значений `4`, затем четыре октета.

### ETH1

Основные имена:

```text
IP.TXT
SUBNET.TXT
GATE.TXT
```

Также принимаются:

```text
IP1.TXT
SUBNET1.TXT
GATE1.TXT
GW.TXT
GW1.TXT
```

Пример `IP.TXT`:

```text
4
192
168
1
1
```

### ETH2

```text
IP2.TXT
SUBNET2.TXT
GATE2.TXT
```

Для gateway также принимается `GW2.TXT`.

Пример `IP2.TXT`:

```text
4
192
168
1
2
```

### Subnet и gateway

```text
SUBNET.TXT / SUBNET2.TXT
4
255
255
255
0
```

```text
GATE.TXT / GATE2.TXT
4
192
168
1
254
```

Если файл отсутствует или содержит ошибку, соответствующий параметр сохраняет значение по умолчанию. IP, subnet и gateway загружаются независимо.

## Значения по умолчанию

```text
ETH1 IP:      192.168.1.1
ETH2 IP:      192.168.1.2
Subnet:       255.255.255.0
Gateway:      192.168.1.254
TCP port:     502
```

MAC-адреса различаются последним байтом:

```text
ETH1: 02:4C:43:50:00:01
ETH2: 02:4C:43:50:00:02
```

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

Пример:

```text
0x0005 = valid + physical port present
0x0306 = timeout + connection_lost + physical port present
```

## Protocol engine

Разделение слоёв:

```text
hal/w5500_lite.*
    raw W5500 socket 0, TCP receive/send, link and socket state

protocol/modbus_tcp/modbus_tcp_server.*
    MBAP framing, stream buffering, FC03 and exception responses

app/ethernet/ethernet_network_config.*
    чтение IP/subnet/gateway с microSD

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

- фактически применённые IP/subnet/gateway;
- имя и результат каждого TXT-файла;
- VERSIONR W5500;
- link up/down;
- socket status;
- количество запросов, ответов, exception и malformed frames;
- текущие 12 holding registers.

`eth reload` повторно читает файлы и аппаратно переинициализирует оба W5500.

## Проверка

1. Скопировать нужные файлы из `RTOS/sd_card` в корень microSD либо оставить существующие совместимые файлы.
2. Подключить ETH1 или ETH2 к компьютеру или коммутатору.
3. Настроить компьютер в соответствующей подсети.
4. Выполнить `eth` и проверить `VERSIONR=0x04`, `init=ok`, `link=up` и фактический IP.
5. Подключиться Modbus TCP client к порту 502.
6. Выполнить FC03, start address 0, quantity 12.
7. Проверить рост `requests` и `responses` в команде `eth`.
8. Сравнить регистры 0..11 с командой `field`.
9. Повторить тест на втором Ethernet-интерфейсе.

Для первичной проверки можно читать только два регистра:

```text
FC03
start address: 0
quantity: 2
```

При запросе за пределами 0..11 server возвращает Modbus exception `0x02 Illegal Data Address`. Неподдерживаемая функция возвращает `0x01 Illegal Function`.
