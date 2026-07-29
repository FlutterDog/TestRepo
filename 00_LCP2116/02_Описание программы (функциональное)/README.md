# LCP Basic Diagnostic Firmware 1.02.0

**Функциональное описание программы и справочник по исходному коду LCP2116**

Целевая платформа: **ATSAM3X8E / ARM Cortex-M3**  
Операционная система: **FreeRTOS**  
Проверенная версия firmware: **1.02.0**

---

## Назначение документации

Эта Doxygen-документация описывает внутреннюю программную архитектуру базовой
прошивки контроллера LCP2116. Она предназначена для разработчика и инженера
сопровождения, которому необходимо:

- найти реализацию конкретной подсистемы;
- понять публичный интерфейс модуля;
- проследить владение UART, SPI и другими аппаратными ресурсами;
- расширить X2X, FieldSensor или Modbus TCP без нарушения базовой архитектуры;
- проверить взаимосвязь конфигурации, runtime-сервисов и диагностики;
- сопровождать firmware после передачи проекта.

Doxygen не заменяет пользовательское руководство контроллера и не является
производственной тестовой утилитой. Проверка платы и формирование PASS/FAIL
выполняются отдельным host-side приложением поверх интерфейсов firmware.

---

## Статус версии

| Параметр | Значение |
|---|---|
| Firmware | LCP Basic Diagnostic Firmware |
| Версия | 1.02.0 |
| Stage | Release 1.02.0 |
| MCU | ATSAM3X8E |
| Архитектура | ARM Cortex-M3 |
| RTOS | FreeRTOS |
| Проект | `LCP_Basic.cppproj` |
| Проверенный tag | `v1.02.0` |
| Проверенный firmware commit | `be2071b2e307f4d76bafe63cc59804982a7552d8` |

Документационные исправления после тега не изменяют бинарное поведение firmware
1.02.0. Любое функциональное изменение прошивки должно оформляться новой
версией и отдельной регрессионной проверкой.

---

## Как пользоваться HTML-справочником

Начинать следует с раздела **«Файлы»**. Для embedded C/C++ проекта это основной
способ навигации: значительная часть интерфейсов реализована свободными
функциями, а не классами.

Рекомендуемый порядок:

1. открыть `main.cpp`;
2. открыть `app/app.hpp` и `app/app.cpp`;
3. посмотреть конфигурацию в `app/config/`;
4. посмотреть аппаратную привязку в `board/`;
5. открыть требуемый HAL-драйвер в `hal/`;
6. перейти к прикладному service в `app/`;
7. использовать раздел **«Классы»** для структур runtime и C++-обёрток;
8. использовать source browser для перехода от объявления к реализации.

Основные точки входа:

| Файл | Назначение |
|---|---|
| `main.cpp` | запуск FreeRTOS и создание LCP task |
| `app/app.cpp` | инициализация и основной неблокирующий цикл |
| `app/diagnostics/diagnostic_console.cpp` | USB service console |
| `app/config/lcp_config_service.cpp` | A/B-хранилище конфигурации |
| `app/field/field_sensor_service.cpp` | четыре Modbus RTU master S1–S4 |
| `app/ethernet/ethernet_modbus_service.cpp` | два Modbus TCP server |
| `app/x2x/x2x_service.cpp` | master внутренней шины X2X |
| `board/` | соответствие логических интерфейсов аппаратным ресурсам |
| `hal/` | регистры ATSAM3X8E, SC16IS7xx и W5500 |
| `protocol/` | повторно используемые Modbus RTU/TCP engines |

---

## Структура исходного проекта

```text
.
├── Config/
│   └── FreeRTOSConfig.h
│
├── app/
│   ├── app.cpp
│   ├── app.hpp
│   ├── version.hpp
│   ├── config/
│   ├── diagnostics/
│   ├── ethernet/
│   ├── field/
│   └── x2x/
│       └── modules/
│
├── board/
│   ├── lcp_board.*
│   ├── lcp_battery.*
│   ├── lcp_ethernet.*
│   ├── lcp_field_ports.*
│   ├── lcp_rs485.*
│   ├── lcp_sc16is.*
│   ├── lcp_sd.*
│   ├── lcp_usb_identity.hpp
│   └── lcp_x2x_port.*
│
├── hal/
│   ├── sam3x_device.hpp
│   ├── sam3x_gpio.*
│   ├── sam3x_internal_flash.*
│   ├── sam3x_rtc.*
│   ├── sam3x_spi.*
│   ├── sam3x_tick.*
│   ├── sam3x_uart.*
│   ├── sam3x_watchdog.*
│   ├── sc16is7xx.*
│   └── w5500_lite.*
│
├── libs/
│   ├── lcp_crc32/
│   ├── lcp_sd_storage/
│   └── SAM_USB/                 excluded from public Doxygen
│
├── middleware/
│   └── FreeRTOS-Kernel/         excluded from public Doxygen
│
├── platform/
│   ├── platform.hpp
│   ├── binary_constants.hpp
│   ├── print.*
│   ├── serial_port.*
│   ├── spi.*
│   ├── platform_gpio.cpp
│   ├── platform_serial.cpp
│   ├── platform_spi.cpp
│   ├── platform_time.cpp
│   ├── newlib_stubs.c
│   └── syscalls.c
│
├── protocol/
│   ├── modbus_rtu/
│   └── modbus_tcp/
│
├── Device_Startup/              excluded from public Doxygen
├── main.cpp
└── LCP_Basic.cppproj
```

---

## Программные уровни

```text
HAL
    регистры MCU и физические аппаратные операции

board
    разводка и ресурсы конкретной платы LCP2116

platform
    общий API времени, GPIO, UART, SPI и форматированного вывода

protocol
    независимые Modbus RTU и Modbus TCP state machines

service
    применение transport и protocol для конкретной функции

diagnostics
    service console, состояние и команды управления
```

Низкоуровневый модуль не должен знать прикладную карту данных. Например:

- `ModbusRtuMaster` не знает модель внешнего прибора;
- W5500 HAL не знает карту FieldSensor;
- X2X-драйвер не читает microSD или Flash самостоятельно;
- консоль вызывает публичный API service и не изменяет private runtime напрямую.

---

## Модель исполнения FreeRTOS

Firmware использует одну основную прикладную задачу LCP.

```text
main.cpp
    -> app_rtos_start()
        -> LCP task
            -> setup()
            -> loop()
            -> vTaskDelay(1 ms)
```

Одна задача обеспечивает:

- однозначное владение UART и SPI;
- предсказуемый порядок обслуживания;
- отсутствие конкурирующего доступа к protocol objects;
- минимальное количество mutex и очередей;
- контролируемое использование памяти.

Каждая длительная операция реализуется как неблокирующая state machine. Нельзя
использовать busy-wait, длительный `delay()` или ожидание аппаратного события без
timeout внутри основного цикла.

Последовательность обслуживания включает:

```text
PLC OK
USB machine configuration transport
USB text console
microSD
A/B configuration service
X2X
FieldSensor S1–S4
Ethernet ETH1/ETH2
X2X fallback echo
HMI diagnostic echo
battery
RTC
watchdog
```

---

## Платформенная конфигурация

Нормализованная `LcpConfigBundle` schema-v1 содержит:

- baudrate и parity S1–S4;
- MAC, IP, subnet и gateway ETH1/ETH2;
- список до шести X2X-модулей;
- совместимые поля EIA;
- зарезервированные байты.

Источники при запуске:

```text
1. наиболее новый валидный A/B-слот внутренней Flash;
2. первоначальный полный импорт с microSD при пустой Flash;
3. встроенные runtime defaults.
```

Внутренняя Flash использует два слота по 16 KiB. Новая запись выполняется в
неактивный слот; commit page записывается последней. При потере питания
незавершённая запись игнорируется, а предыдущее поколение сохраняется.

Основные файлы:

```text
app/config/lcp_config_bundle.*
app/config/lcp_config_service.*
hal/sam3x_internal_flash.*
libs/lcp_crc32/*
```

---

## USB-интерфейсы

Один USB CDC-порт используется в двух режимах:

```text
text mode
    service console

machine mode
    бинарный configuration protocol version 1
```

Машинный кадр начинается с `00 4C 43 50`. Поддерживаются `HELLO`,
`GET_CONFIG`, `VALIDATE_CONFIG`, `PUT_CONFIG`, `GET_STATUS`, `REBOOT` и `EXIT`.

Reference Python utility находится в `tools/`, но не включается в Doxygen как
часть firmware-кода. Она является примером host-side интеграции с опубликованным
USB-протоколом.

---

## FieldSensor и Modbus RTU

S1–S4 работают как четыре независимых Modbus RTU master. Каждый порт имеет:

- собственный transport;
- собственный `ModbusRtuMaster`;
- RX/TX state machine;
- runtime quality;
- success/error counters;
- timeout и период опроса.

Физическое соответствие:

```text
S1 -> SC16IS7xx
S2 -> ATSAM3X8E UART1
S3 -> ATSAM3X8E UART3
S4 -> ATSAM3X8E UART2
```

Универсальный protocol engine расположен в `protocol/modbus_rtu/`, а параметры
демонстрационного прибора и обработка результата — в `app/field/`.

---

## Ethernet и Modbus TCP

Контроллер содержит два независимых W5500. Каждый интерфейс имеет собственные:

- сетевые настройки;
- socket 0;
- `ModbusTcpServer`;
- RX/TX buffers;
- диагностические счётчики.

Baseline публикует одинаковую holding map FieldSensor на ETH1 и ETH2 через TCP
port 502. W5500 transport находится в `hal/w5500_lite.*`, protocol engine — в
`protocol/modbus_tcp/`, а прикладная карта — в `app/ethernet/`.

---

## Внутренняя шина X2X

LCP2116 выполняет роль X2X master и последовательно обслуживает до шести внешних
модулей.

```text
active LcpConfigBundle
    -> x2x_config
        -> x2x_registry
            -> x2x_catalog
                -> x2x_service
                    -> driver модуля
                        -> ModbusRtuMaster
```

Новый тип модуля обычно добавляется в:

```text
app/x2x/x2x_types.hpp
app/x2x/modules/x2x_module_drivers.hpp
app/x2x/modules/x2x_<module>.cpp
app/x2x/x2x_catalog.cpp
LCP_Basic.cppproj
```

Стабильные числовые ID являются частью внешнего формата и после публикации не
должны переиспользоваться.

---

## Другие аппаратные сервисы

### HMI

HMI — отдельный RS-485 интерфейс на SC16IS7xx. В baseline он принадлежит
неблокирующему diagnostic echo service. Перед передачей HMI прикладному
протоколу echo service необходимо отключить, сохранив единственного владельца
порта.

### microSD

Поддерживаются FAT16/FAT32, корневой каталог и короткие имена FAT 8.3. Полный
канонический конфигурационный комплект принимается только целиком.

### RTC и батарея

Используется внутренний RTC ATSAM3X8E и резервное питание VDDBU. Диагностика
проверяет чтение, установку времени и состояние батареи.

### Watchdog

Аппаратный watchdog включён постоянно. Команда контролируемого теста прекращает
подачу watchdog и должна приводить к ожидаемому hardware reset с последующим
восстановлением USB CDC.

---

## Правила расширения

При добавлении нового service необходимо:

1. определить владельца физического интерфейса;
2. отделить transport от protocol и прикладной логики;
3. реализовать `init()` и короткий неблокирующий `poll()`;
4. добавить timeout и явные коды результата;
5. сохранить last-good data отдельно от quality;
6. добавить профильную диагностику и команду консоли при необходимости;
7. добавить исходный `.cpp` в `LCP_Basic.cppproj`;
8. обновить Doxygen-комментарии и пользовательский README;
9. собрать Debug и Release;
10. выполнить регрессионную проверку затронутых интерфейсов.

Изменение общего protocol layer требует проверки всех существующих его
потребителей.

---

## Исключённые каталоги

Публичный HTML намеренно не включает:

- `middleware/FreeRTOS-Kernel` — сторонний RTOS kernel;
- `libs/SAM_USB` — platform support для USB CDC;
- `Device_Startup` — startup и system-файлы ATSAM3X8E;
- `tools` — host-side Python utility;
- `sd_card` — пример внешнего конфигурационного комплекта;
- Debug/Release artifacts.

Эти файлы остаются частью передаваемого проекта там, где это требуется для
сборки или эксплуатации, но не смешиваются со справочником собственного кода
LCP Basic.

---

## Генерация документации

Подготовка рабочей папки и команды запуска описаны на отдельной странице
`DOXYGEN_COMMANDS.md`.

Рекомендуемый запуск из `C:\DoxygenFolder`:

```bat
build_doxygen.bat
```

Результат:

```text
docs\doxygen\html\index.html
```
