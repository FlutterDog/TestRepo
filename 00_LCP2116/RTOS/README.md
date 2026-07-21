<p align="center">
  <img src="IMG/IGAS%20Logo%20GIT.png" alt="IGAS" width="210">
</p>
<br>
<p align="center">
  <img src="IMG/01.%20StartPage.png" alt="Плата LDO1128" width="450">
</p>
<br>

---

<div align="center">

# Lorentz

**Свободнопрограммируемый контроллер LCP2116**  
**Руководство пользователя и разработчика**

---

Версия прошивки: **1.00.0**  
Целевая платформа: **ATSAM3X8E / ARM Cortex-M3**

</div>

---

<a id="contents"></a>

## Оглавление

1. [Общие сведения о платформе Lorentz](#platform-overview)
2. [Назначение базовой прошивки](#firmware-purpose)
3. [Состав аппаратных и программных интерфейсов](#interfaces)
4. [Быстрый старт](#quick-start)
5. [Архитектура программного обеспечения](#architecture)
6. [Модель исполнения FreeRTOS](#freertos-model)
7. [Сборка и прошивка](#build)
8. [Подготовка microSD](#microsd)
9. [Служебная консоль USB](#console)
10. [Команды и диагностика](#commands)
11. [Порты S1–S4 и пример FieldSensor](#fieldsensor)
12. [Универсальный интерфейс HMI](#hmi)
13. [Подключение нового Modbus RTU-прибора](#new-field-device)
14. [Ethernet и Modbus TCP](#ethernet)
15. [Внутренняя шина X2X](#x2x)
16. [Добавление нового X2X-модуля](#new-x2x-module)
17. [Разработка неблокирующего X2X-драйвера](#x2x-driver)
18. [Добавление команды служебной консоли](#new-console-command)
19. [Диагностика и поиск неисправностей](#troubleshooting)
20. [Правила разработки](#development-rules)
21. [Проверка перед выпуском](#release-checklist)
22. [Подтверждённое состояние версии 1.00.0](#release-verification)
23. [Архитектурные решения и качество кода](#quality-review)

> 🟦 **Информация**
>
> Все пункты оглавления являются кликабельными. В веб-интерфейсе GitHub также доступно автоматическое меню **Outline**, которое формируется по заголовкам документа.

### Условные обозначения

> 🟦 **Информация**
>
> Дополнительные пояснения, рекомендуемый порядок действий и особенности реализации.

> ⚠️ **Внимание**
>
> Условия, нарушение которых может привести к ошибке конфигурации, блокировке цикла обслуживания, повреждению данных либо неправильной интерпретации состояния системы.

---

<a id="platform-overview"></a>

## 1. Общие сведения о платформе Lorentz

Lorentz представляет собой свободнопрограммируемую модульную систему управления, предназначенную для построения распределённых систем автоматизации, сбора данных, контроля технологического оборудования и управления исполнительными устройствами.

Центральным элементом системы является контроллер **LCP2116**. Он выполняет прикладную программу, обслуживает внутреннюю модульную шину, взаимодействует с внешними приборами и предоставляет сетевые и сервисные интерфейсы.

Функциональные модули Lorentz подключаются к контроллеру по внутренней шине X2X. В зависимости от проекта могут использоваться модули дискретных входов, дискретных выходов, аналоговых входов, контроля коммутационного ресурса и другие специализированные устройства.

Конкретная карта регистров, алгоритмы и эксплуатационная документация каждого функционального модуля находятся в его собственном репозитории. В данном руководстве модули рассматриваются только с точки зрения подключения к LCP2116, регистрации в X2X и разработки драйвера центрального контроллера.

> 🟦 **Информация**
>
> Базовая прошивка не привязывает платформу Lorentz к конкретной технологической установке. Прикладная логика формируется разработчиком на основе предоставленных интерфейсов, сервисов и примеров.

[↑ К оглавлению](#contents)

---

<a id="firmware-purpose"></a>

## 2. Назначение базовой прошивки

`LCP Basic Diagnostic Firmware` является исходной программной платформой центрального контроллера LCP2116. Прошивка одновременно выполняет 2 функции:

1. обеспечивает проверку и диагностику аппаратных ресурсов контроллера;
2. предоставляет готовую архитектуру для дальнейшей разработки прикладного программного обеспечения.

В версии 1.00.0 реализованы:

- FreeRTOS;
- одна основная прикладная задача LCP;
- неблокирующая модель обслуживания подсистем;
- X2X master на UART0;
- статический каталог и реестр X2X-модулей;
- 4 независимых Modbus RTU master на портах S1–S4;
- диагностический echo-test универсального интерфейса HMI;
- 2 независимых Modbus TCP server на ETH1 и ETH2;
- чтение конфигурации с microSD;
- служебная консоль USB CDC;
- RTC;
- аппаратный watchdog;
- диагностика памяти, UART, microSD, X2X, FieldSensor и Ethernet;
- примеры добавления внешнего прибора и нового X2X-модуля.

Прошивка организована как набор уровней с определённой ответственностью. Универсальные реализации протоколов отделены от аппаратной привязки и прикладных сервисов. Благодаря этому новый прибор или модуль добавляется на уровне собственного драйвера и сервиса без переработки общего Modbus engine.

[↑ К оглавлению](#contents)

---

<a id="interfaces"></a>

## 3. Состав аппаратных и программных интерфейсов

LCP2116 построен на микроконтроллере ATSAM3X8E с ядром ARM Cortex-M3. Базовая аппаратная платформа предоставляет:

```text
X2X     внутренняя модульная шина Lorentz
S1      универсальный RS-485
S2      универсальный RS-485
S3      универсальный RS-485
S4      универсальный RS-485
HMI     универсальный RS-485
ETH1    Ethernet на W5500
ETH2    Ethernet на W5500
USB     служебная консоль USB CDC
microSD конфигурационные файлы FAT16/FAT32
RTC     часы реального времени
WDT     аппаратный watchdog
```

Назначение интерфейсов в базовой прошивке:

```text
X2X     -> Modbus RTU master внутренней модульной шины
S1..S4  -> независимые FieldSensor Modbus RTU master
HMI     -> неблокирующий диагностический echo-test
ETH1    -> Modbus TCP server, TCP port 502
ETH2    -> Modbus TCP server, TCP port 502
USB     -> служебная консоль и диагностика
```

> ⚠️ **Внимание**
>
> Один физический half-duplex UART должен иметь только одного владельца. Нельзя одновременно запускать echo-test и прикладной protocol engine на одном интерфейсе.

[↑ К оглавлению](#contents)

---

<a id="quick-start"></a>

## 4. Быстрый старт

Минимальная последовательность ввода прошивки в работу:

1. Открыть проект Microchip Studio:

   ```text
   00_LCP2116/RTOS/LCP_Basic.cppproj
   ```

2. Выбрать конфигурацию `Release`.
3. Собрать проект.
4. Проверить отсутствие ошибок компиляции и линковки.
5. Скопировать требуемые конфигурационные файлы из каталога `sd_card/` в корень microSD.
6. Установить microSD в контроллер.
7. Прошить LCP2116.
8. Подключить служебную консоль USB CDC.
9. Выполнить:

   ```text
   version
   help
   status
   rtos
   sd
   field
   x2x
   eth
   sc16is
   time
   watchdog
   ```

Ожидаемая версия:

```text
version = 1.00.0
stage = Release 1.00.0
target = ATSAM3X8E
```

Для быстрой проверки основных интерфейсов используются:

```text
field   состояние портов S1–S4
x2x     состояние внутренней модульной шины
eth     состояние ETH1/ETH2 и карта Modbus TCP
sc16is  состояние внешних UART, включая HMI и S1
rtos    SRAM, heap и stack основной задачи
```

> ⚠️ **Внимание**
>
> Перед первым запуском необходимо проверить содержимое microSD. Некорректные имена файлов, неправильный порядок значений или несоответствие параметров UART подключённому прибору приводят к применению параметров по умолчанию либо к отсутствию связи.

[↑ К оглавлению](#contents)

---

<a id="architecture"></a>

## 5. Архитектура программного обеспечения

### 5.1 Структура проекта

```text
main.cpp
    точка входа и запуск RTOS

app/
    прикладные сервисы, runtime-состояния и диагностика

board/
    привязка логических интерфейсов к ресурсам платы LCP2116

hal/
    низкоуровневая работа ATSAM3X8E, SC16IS7xx и W5500

protocol/
    универсальные реализации Modbus RTU и Modbus TCP

platform/
    Serial, SPI, GPIO, millis и другие platform-функции

libs/
    независимые повторно используемые библиотеки

sd_card/
    штатные примеры конфигурационных файлов microSD

IMG/
    изображения, используемые данным README
```

### 5.2 Назначение уровней

```text
HAL       регистры MCU и физические аппаратные операции
board     разводка и ресурсы конкретной платы
protocol  универсальная реализация протокола
service   применение протокола для конкретной функции
status    диагностический вывод и обработка сервисных команд
```

Пример прохождения данных FieldSensor:

```text
UART / SC16IS
    -> board transport
        -> ModbusRtuMaster
            -> FieldSensor service
                -> диагностический status
                -> Modbus TCP holding map
```

Пример прохождения данных X2X:

```text
UART0
    -> ModbusRtuMaster
        -> X2X service
            -> driver конкретного типа
                -> runtime-структура модуля
                    -> служебная консоль
```

### 5.3 Владение интерфейсами

```text
UART0       X2X master
            либо fallback echo при пустой X2X-конфигурации

S1..S4      FieldSensor Modbus RTU master

HMI         диагностический echo-test

ETH1        отдельный Modbus TCP server
ETH2        отдельный Modbus TCP server
```

### 5.4 Правила разделения

Низкоуровневый драйвер не должен зависеть от прикладной структуры данных:

- W5500 HAL не знает карту FieldSensor;
- Modbus RTU master не знает модель прибора;
- X2X-драйвер не читает microSD самостоятельно;
- служебная консоль использует публичный API сервиса;
- прикладной сервис не управляет регистрами MCU напрямую, если существует HAL или board layer.

Такое разделение упрощает аппаратную проверку, повторное использование кода и добавление новых устройств.

[↑ К оглавлению](#contents)

---

<a id="freertos-model"></a>

## 6. Модель исполнения FreeRTOS

### 6.1 Основной цикл

Прошивка использует одну основную прикладную задачу LCP:

```text
main.cpp
  -> app_rtos_start()
      -> LCP FreeRTOS task
          -> setup()
          -> постоянный цикл
              -> microSD service
              -> X2X service
              -> FieldSensor S1–S4
              -> Ethernet ETH1/ETH2
              -> HMI diagnostic echo
              -> RTC
              -> USB console
              -> watchdog
```

После каждого прохода задача освобождает процессор минимум на 1 tick. Все прикладные сервисы вызываются последовательно и выполняют ограниченный объём работы за один вызов.

### 6.2 Неблокирующая модель

Длительная операция представляется машиной состояний:

```text
IDLE
    проверить возможность запуска
    начать операцию
    перейти в WAIT
    вернуть управление

WAIT
    проверить состояние
    при необходимости продолжить ожидание
    обработать результат или timeout
    перейти в IDLE
```

Каждый сервис должен иметь:

- явное runtime-состояние;
- функцию инициализации;
- неблокирующую функцию `poll()`;
- timeout аппаратных и протокольных операций;
- явный код результата;
- определённое поведение при ошибке и восстановлении.

> ⚠️ **Внимание**
>
> Блокирующая функция внутри основной задачи останавливает обслуживание X2X, FieldSensor, Ethernet, HMI, RTC, USB-консоли и watchdog. Не допускаются длительные `delay`, busy-wait и циклы ожидания аппаратного события без timeout.

### 6.3 Причины использования одной прикладной задачи

Единая задача обеспечивает:

- однозначное владение UART и SPI;
- отсутствие конкурирующего доступа к протокольным объектам;
- предсказуемый порядок вызовов;
- минимальное количество mutex и очередей;
- контролируемое использование памяти;
- простую диагностику времени выполнения.

Отдельную FreeRTOS-задачу следует вводить только при наличии конкретной необходимости: независимого временного ограничения, блокирующего внешнего API либо значительного объёма вычислений, который невозможно корректно разделить на шаги.

### 6.4 Контроль памяти

Команда:

```text
rtos
```

показывает:

- общий объём SRAM;
- назначенные и свободные linker-секции;
- `.data` и `.bss`;
- общий размер FreeRTOS heap;
- текущий и минимальный свободный heap;
- размер stack основной задачи;
- пиковое использование stack;
- значения в байтах и процентах.

Признаки стабильной работы:

- текущий свободный heap не уменьшается после одинаковых операций;
- minimum-ever-free стабилизируется после начального прогона;
- stack сохраняет достаточный резерв;
- uptime не сбрасывается без штатной причины.

[↑ К оглавлению](#contents)

---

<a id="build"></a>

## 7. Сборка и прошивка

### 7.1 Проект

```text
00_LCP2116/RTOS/LCP_Basic.cppproj
```

Целевая платформа:

```text
ATSAM3X8E
ARM Cortex-M3
```

### 7.2 Рекомендуемый порядок сборки

1. Собрать `Debug`.
2. Просмотреть ошибки и предупреждения.
3. Собрать `Release`.
4. Проверить использование Flash и SRAM.
5. Прошить Release-бинарник.
6. Выполнить минимальный smoke test.

В Build Output должны присутствовать:

```text
Build succeeded
0 failed
```

Дополнительно фиксируются:

```text
warnings
text / Flash
.data
.bss / SRAM
```

> 🟦 **Информация**
>
> Стандартное сообщение Atmel Studio о приблизительной оценке memory usage относится к методике расчёта секций ELF и не является предупреждением исходного кода.

### 7.3 Добавление нового исходного файла

После создания нового `.cpp` его необходимо включить в `LCP_Basic.cppproj` через Microchip Studio либо вручную:

```xml
<Compile Include="app\x2x\modules\x2x_example.cpp">
    <SubType>compile</SubType>
</Compile>
```

> ⚠️ **Внимание**
>
> Наличие файла в каталоге репозитория не означает, что он включён в сборку Microchip Studio. После добавления нового `.cpp` необходимо проверить фактическую команду компилятора в Build Output.

### 7.4 Прошивка и загрузчик

USB CDC сохраняет штатный переход в ROM-загрузчик SAM-BA при открытии порта на 1200 baud. После прошивки необходимо проверить:

- запуск планировщика;
- появление USB CDC;
- выполнение команды `version`;
- отсутствие цикла перезагрузки;
- возможность повторной штатной прошивки.

[↑ К оглавлению](#contents)

---

<a id="microsd"></a>

## 8. Подготовка microSD

Прошивка поддерживает FAT16/FAT32. Штатные примеры находятся в каталоге:

```text
00_LCP2116/RTOS/sd_card/
```

Файлы копируются в корень карты.

### 8.1 FieldSensor

```text
baud.TXT
Parity.TXT
```

### 8.2 Ethernet

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

### 8.3 X2X

```text
X2X.TXT
```

### 8.4 EIA.TXT

```text
EIA.TXT
```

Файл сохраняется в комплекте конфигурации. В текущем базовом примере логические потоки FieldSensor закреплены непосредственно за S1–S4, поэтому маршрутизация из `EIA.TXT` не применяется.

### 8.5 Правила текстового формата

Общий числовой parser поддерживает:

- UTF-8 BOM;
- пустые строки;
- комментарии после `//`;
- комментарии после `#`;
- завершающую строку `fin` или `Fin`;
- строгую проверку количества значений;
- проверку лишних и недостающих значений;
- проверку numeric overflow.

Имена файлов являются частью внешнего формата. Регистр символов имеет значение.

Правильно:

```text
baud.TXT
Parity.TXT
IP.txt
```

Неправильно:

```text
BAUD.TXT
PARITY.TXT
IP.TXT
```

> ⚠️ **Внимание**
>
> Не следует создавать альтернативные имена или дубли файлов. В Windows необходимо проверить, что расширение не было скрыто и файл не получил имя вида `IP.txt.txt`.

### 8.6 Проверка microSD

```text
sd
```

Команда показывает наличие карты, состояние файловой системы и тип FAT.

```text
sd test
```

Команда создаёт и читает `SDTEST.TXT`.

> ⚠️ **Внимание**
>
> `sd test` изменяет содержимое карты и предназначена для сервисной проверки.

[↑ К оглавлению](#contents)

---

<a id="console"></a>

## 9. Служебная консоль USB

### 9.1 Подключение

Прошивка создаёт USB CDC-порт. Для подключения можно использовать PuTTY, Tera Term, RealTerm или другой последовательный терминал.

Рекомендуемые настройки:

```text
115200
8 data bits
no parity
1 stop bit
no flow control
```

Для USB CDC выбранный baudrate не определяет физическую скорость передачи, однако 115200 используется как стандартная настройка терминала.

После подключения отображается:

```text
LCP firmware ready. Type 'help' or '?'.
```

Команды не чувствительны к регистру и выполняются клавишей Enter.

### 9.2 Формат вывода

Именованные параметры:

```text
name = value
name = value, next = value
```

Индексированные массивы:

```text
0: value, 1: value
```

Двоеточия внутри значения сохраняются:

```text
mac = DE:AD:BE:EF:FE:EE
datetime = 2026-07-20 21:30:00
```

Общие функции форматирования находятся в:

```text
app/diagnostics/diagnostic_output.hpp
```

### 9.3 Работа со справкой

```text
help
```

Короткая форма:

```text
?
```

`help` выводит сгруппированный список доступных команд. Для получения полного состояния используется `status`. Для регулярной диагностики рекомендуется вызывать профильные команды `field`, `x2x`, `eth`, `sc16is`, `rtc`, `rtos` и другие.

[↑ К оглавлению](#contents)

---

<a id="commands"></a>

## 10. Команды и диагностика

### 10.1 Основные команды

| Команда | Назначение |
|---|---|
| `help`, `?` | Список команд по функциональным группам |
| `version`, `ver` | Версия, stage и целевой MCU |
| `status` | Полный диагностический отчёт |
| `uptime` | Время работы в миллисекундах и формате `D d HH:MM:SS` |
| `rtos` | SRAM, heap и stack в байтах и процентах |
| `periodic on` | Полный `status` каждые 10 секунд |
| `periodic off` | Отключение периодического отчёта |

### 10.2 X2X

| Команда | Назначение |
|---|---|
| `x2x` | Конфигурация и состояние модулей |
| `x2x reload` | Проверка и применение `X2X.TXT` |
| `x2x pause` | Завершение активного цикла и остановка опроса |
| `x2x resume` | Возобновление опроса |
| `x2x ldo SLAVE VALUE` | Сервисная установка локального регистра выхода LDO |

### 10.3 FieldSensor

| Команда | Назначение |
|---|---|
| `field` | Состояние S1–S4 |
| `field reload` | Повторное чтение `baud.TXT` и `Parity.TXT` |
| `field pause` | Запрет запуска новых запросов |
| `field resume` | Возобновление опроса |

### 10.4 Ethernet

| Команда | Назначение |
|---|---|
| `eth` | Состояние ETH1/ETH2 и holding registers |
| `eth reload` | Повторное чтение сетевых файлов и перезапуск W5500 |

### 10.5 Аппаратные и сервисные команды

| Команда | Назначение |
|---|---|
| `rs485` | Владение встроенными RS-485 и аппаратные ошибки |
| `sc16is` | Обнаружение и назначение внешних UART |
| `sd` | Состояние microSD и файловой системы |
| `sd test` | Запись и чтение `SDTEST.TXT` |
| `battery` | Состояние резервной батареи CR2032 |
| `time`, `rtc time` | Краткий вывод даты и времени |
| `rtc` | Полный отчёт RTC |
| `rtc set YYYY-MM-DD HH:MM:SS` | Неблокирующая установка времени |
| `watchdog` | Конфигурация watchdog и причина reset |
| `watchdog feed` | Ручная перезагрузка счётчика watchdog |
| `watchdog test reset` | Контролируемая проверка аппаратного reset |

### 10.6 Полный диагностический отчёт

```text
status
```

Отчёт включает:

- SYSTEM;
- RTOS SUMMARY;
- RS-485 PORTS;
- X2X;
- FieldSensor;
- SC16IS;
- Ethernet;
- microSD;
- battery;
- RTC;
- watchdog.

> ⚠️ **Внимание**
>
> `periodic on` создаёт непрерывный большой поток в USB CDC. Этот режим следует использовать только при целенаправленной диагностике.

[↑ К оглавлению](#contents)

---

<a id="fieldsensor"></a>

## 11. Порты S1–S4 и пример FieldSensor

FieldSensor является базовым примером подключения внешнего Modbus RTU-прибора к пользовательским портам контроллера. Тип прибора, slave address и карта регистров задаются в исходном коде. TXT-файлы на microSD изменяют только аппаратные параметры последовательных портов.

Все 4 порта работают как независимые Modbus RTU master и по умолчанию выполняют одинаковый запрос:

```text
slave address = 1
function = FC03 Read Holding Registers
start register = 0
register count = 2
poll period = 300 ms
timeout = 500 ms
```

### 11.1 Физическое соответствие

```text
S1 -> внешний UART SC16IS7xx
S2 -> ATSAM3X8E UART1 / Serial1
S3 -> ATSAM3X8E UART3 / Serial3
S4 -> ATSAM3X8E UART2 / Serial2
```

Для S1 автоматически определяется канал SC16IS752 или отдельный SC16IS740. Конфигурационный файл `SC16.txt` не используется.

### 11.2 baud.TXT

```text
4
9600
9600
1200
9600
fin
```

Значения расположены в порядке:

```text
S1
S2
S3
S4
```

В файле указываются фактические baudrate, а не индексы таблицы.

### 11.3 Parity.TXT

```text
4
0
0
0
0
fin
```

Допустимые коды:

```text
0 = 8N1
1 = 8O1
2 = 8E1
```

Буквы `N/O/E`, слова `NONE/ODD/EVEN` и ASCII-коды не поддерживаются. Скорость и parity загружаются независимо. Ошибка одного файла не препятствует применению корректных значений из другого.

### 11.4 Runtime каждого порта

Каждый S1–S4 имеет собственные:

- transport callbacks;
- объект `ModbusRtuMaster`;
- RX/TX-буферы протокола;
- 2 регистра результата;
- таймер следующего запроса;
- timeout;
- признаки качества;
- счётчики успешных и неуспешных транзакций;
- время последнего успешного обновления.

Отдельные FreeRTOS-задачи для портов не создаются. Один `field_sensor_service_poll()` последовательно выполняет неблокирующий шаг для каждого порта.

### 11.5 Качество данных

После 5 последовательных ошибок устанавливаются:

```text
connection = lost
valid = no
```

Последние корректно принятые значения сохраняются как last-good data. Первый корректный ответ автоматически восстанавливает:

```text
connection = online
valid = yes
consecutive_failures = 0
```

> ⚠️ **Внимание**
>
> Последнее сохранённое значение нельзя использовать без проверки `valid` и состояния связи.

### 11.6 Reload и pause

```text
field reload
```

Активные транзакции сначала завершаются, после чего UART переинициализируются.

```text
field pause
field resume
```

Pause запрещает запуск новых запросов, но не обрывает активный RTU-кадр.

### 11.7 Проверка порта

1. Подключить Modbus RTU slave.
2. Установить slave address 1.
3. Настроить baudrate и parity соответствующего порта.
4. Выполнить `field`.
5. Проверить переход в `online` и рост `success`.
6. Изменить значения регистров и проверить их обновление.
7. Отключить линию.
8. Проверить `connection = lost` и `valid = no`.
9. Восстановить линию.
10. Проверить автоматический возврат в `online`.

### 11.8 Альтернативный slave-режим

Повторно используемая реализация FC03 slave находится в:

```text
protocol/modbus_rtu/modbus_rtu_slave.cpp
protocol/modbus_rtu/modbus_rtu_slave.hpp
```

Отключённый пример подключения:

```text
app/field/field_sensor_slave_example.cpp
```

При включении slave-режима необходимо изменить владельца соответствующего физического порта и отключить master на этом UART.

[↑ К оглавлению](#contents)

---

<a id="hmi"></a>

## 12. Универсальный интерфейс HMI

HMI является отдельным пользовательским интерфейсом RS-485. Название разъёма отражает первоначальное назначение, но программно порт не закреплён за панелью оператора.

В базовой прошивке HMI используется для неблокирующего диагностического echo-test:

```text
baudrate = 9600
frame = 8N1
interframe gap = 5 ms
response delay = 10 ms
maximum frame size = 128 bytes
maximum RX processing per poll = 16 bytes
TX timeout = 1000 ms
```

### 12.1 Алгоритм работы

1. В каждом цикле `poll()` из FIFO принимается ограниченное количество байтов.
2. После паузы не менее 5 ms принятый набор байтов считается завершённым кадром.
3. Через 10 ms начинается возврат кадра.
4. Если FIFO передатчика принимает только часть данных, передача продолжается в следующих циклах с сохранённого смещения.
5. Если передача не завершается за 1000 ms, кадр сбрасывается без блокировки основной задачи.

### 12.2 Поддерживаемые аппаратные варианты

```text
SC16IS740
    HMI -> UART2_CS, channel A

SC16IS752
    HMI -> UART2_CS, channel A
    S1  -> UART2_CS, channel B
```

Выбор выполняется автоматически при старте по результатам проверки scratchpad-регистра.

### 12.3 Файлы реализации

```text
board/lcp_sc16is.hpp
board/lcp_sc16is.cpp
hal/sc16is7xx.hpp
hal/sc16is7xx.cpp
app/diagnostics/sc16is_echo_test.hpp
app/diagnostics/sc16is_echo_test.cpp
```

Назначение:

```text
lcp_sc16is.*
    определение аппаратной конфигурации и логической карты HMI/S1

sc16is7xx.*
    SPI-доступ, baudrate/frame, FIFO и автоматическое управление RS-485 RTS

sc16is_echo_test.*
    неблокирующий приём кадра и его возврат на HMI
```

> 🟦 **Информация**
>
> В версии 1.00.0 HMI не входит в FieldSensor S1–S4 и не публикуется в карте Modbus TCP `0..11`.

### 12.4 Передача HMI пользовательскому протоколу

1. Исключить HMI из инициализации `sc16is_echo_test_init()`.
2. Получить дескриптор `lcp_sc16is_get_map().hmi`.
3. Настроить порт через `sc16is_begin()`.
4. Создать отдельный неблокирующий service с функциями `init()` и `poll()`.
5. Реализовать timeout, обработку частичного TX и очистку RX.
6. Добавить профильную диагностику.
7. Проверить, что echo-test и новый service не обслуживают HMI одновременно.

> ⚠️ **Внимание**
>
> Передача HMI новому сервису выполняется как смена владельца интерфейса. Одновременная работа echo-test и прикладного протокола не допускается.

[↑ К оглавлению](#contents)

---

<a id="new-field-device"></a>

## 13. Подключение нового Modbus RTU-прибора

### 13.1 Выбор способа интеграции

Для простого прибора, который опрашивается одним запросом FC03, можно изменить существующий FieldSensor example.

Для устройства с несколькими запросами, операциями записи, сложным декодированием или собственной логикой рекомендуется создать отдельный прикладной service. Универсальный `ModbusRtuMaster` при этом не изменяется.

### 13.2 Параметры текущего примера

Файл:

```text
app/field/field_sensor_service.cpp
```

Функция:

```cpp
set_default_configs()
```

Основные параметры:

```cpp
g_configs[index].slave_address
g_configs[index].start_register
g_configs[index].register_count
g_configs[index].poll_period_ms
g_configs[index].timeout_ms
```

Пример:

```cpp
g_configs[index].role = FIELD_PORT_MASTER;
g_configs[index].slave_address = 7U;
g_configs[index].start_register = 100U;
g_configs[index].register_count = 4U;
g_configs[index].poll_period_ms = 1000U;
g_configs[index].timeout_ms = 700U;
```

### 13.3 Размер буфера

Текущее значение:

```cpp
FIELD_SENSOR_REGISTER_COUNT = 2U
```

Файл:

```text
app/field/field_sensor_service.hpp
```

Если прибор возвращает больше регистров, необходимо одновременно проверить:

1. `FIELD_SENSOR_REGISTER_COUNT`;
2. `FieldSensorPortState::registers`;
3. `set_default_configs()`;
4. декодирование данных;
5. диагностический вывод FieldSensor;
6. Ethernet holding map;
7. Modbus TCP-документацию в этом README.

> ⚠️ **Внимание**
>
> Нельзя увеличить только `register_count`, оставив прежний runtime buffer. Такое изменение создаёт риск записи за границы массива.

### 13.4 Когда необходим отдельный service

Отдельный service рекомендуется, если прибор требует:

- нескольких последовательных запросов;
- FC10 или других операций записи;
- собственной state machine;
- преобразования нескольких типов данных;
- масштабирования и проверки диапазона;
- отдельных quality-флагов;
- собственной диагностики;
- независимой карты публикации.

### 13.5 Неблокирующий шаблон сервиса

```text
1. вызвать modbus_rtu_master_poll()
2. проверить завершение активной транзакции
3. обработать result и exception code
4. сохранить данные только после полной проверки ответа
5. запланировать следующий запрос
6. при наступлении срока запустить новую транзакцию
7. вернуть управление основной задаче
```

Не допускаются:

```text
delay(...)
while (ожидание ответа)
busy-wait аппаратного флага
ожидание без timeout
```

[↑ К оглавлению](#contents)

---

<a id="ethernet"></a>

## 14. Ethernet и Modbus TCP

Контроллер имеет 2 независимых W5500. Каждый интерфейс обслуживается отдельным экземпляром `ModbusTcpServer`, собственными RX/TX-буферами и счётчиками.

```text
protocol = Modbus TCP
port = 502
function = FC03 Read Holding Registers
holding registers = 0..11
write functions = not supported
```

ETH1 и ETH2 публикуют одинаковую read-only карту данных FieldSensor.

### 14.1 Конфигурационные файлы

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

Формат IP, SUBNET и GATE:

```text
4
192
168
1
1
fin
```

Формат MAC:

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

MAC задаётся 6 десятичными байтами.

> ⚠️ **Внимание**
>
> Одинаковый IP на ETH1 и ETH2 допустим только при подключении интерфейсов к физически раздельным сетям. При работе в одной LAN должны использоваться разные IP-адреса и уникальные MAC-адреса.

### 14.2 Применение настроек

```text
eth reload
```

Команда повторно читает 8 сетевых файлов и переинициализирует оба W5500.

Проверка:

```text
eth
```

Ожидаемое состояние подключённого интерфейса:

```text
initialized = yes
init = ok
VERSIONR = 0x04
link = up
```

### 14.3 Holding register map

| Адрес | Значение |
|---:|---|
| 0 | S1 value0 |
| 1 | S1 value1 |
| 2 | S1 quality |
| 3 | S2 value0 |
| 4 | S2 value1 |
| 5 | S2 quality |
| 6 | S3 value0 |
| 7 | S3 value1 |
| 8 | S3 quality |
| 9 | S4 value0 |
| 10 | S4 value1 |
| 11 | S4 quality |

Чтение всей карты:

```text
FC03
start address = 0
quantity = 12
```

Некоторые Modbus-клиенты отображают holding registers как `40001`, `40002` и далее. В таком представлении `40001` обычно соответствует protocol offset `0`.

### 14.4 Quality register

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

> ⚠️ **Внимание**
>
> Клиент обязан анализировать quality register. `value0` и `value1` могут содержать последние корректные данные после потери RS-485 связи.

### 14.5 Архитектура Ethernet

```text
hal/w5500_lite.*
    raw socket, TCP receive/send, link и socket state

protocol/modbus_tcp/modbus_tcp_server.*
    MBAP framing, stream buffering, FC03 и exception responses

app/ethernet/ethernet_network_config.*
    чтение MAC/IP/SUBNET/GATE с microSD

app/ethernet/ethernet_modbus_service.*
    2 server instance и прикладная holding map
```

W5500 transport передаёт только целый Modbus TCP ADU. Если TX memory временно недостаточно, сформированный ответ сохраняется и повторяется на следующем `poll()`. Частичный ADU не отправляется.

### 14.6 Расширение карты

Изменяются:

```text
app/ethernet/ethernet_modbus_service.hpp
app/ethernet/ethernet_modbus_service.cpp
app/diagnostics/ethernet_status.cpp
README.md
```

Порядок:

1. определить новую карту адресов;
2. увеличить статический holding buffer;
3. заполнить адреса в `update_holding_map()`;
4. проверить диапазоны в `read_holding()`;
5. обновить `static_assert`;
6. обновить диагностический вывод;
7. обновить данный раздел;
8. проверить минимальный и максимальный адрес;
9. проверить exception response за пределами карты.

[↑ К оглавлению](#contents)

---

<a id="x2x"></a>

## 15. Внутренняя шина X2X

X2X используется для последовательного подключения функциональных модулей Lorentz. Контроллер LCP2116 выполняет роль master и циклически обслуживает модули в порядке, заданном файлом `X2X.TXT`.

### 15.1 Архитектура X2X

```text
X2X.TXT
    -> x2x_config
        -> x2x_registry
            -> x2x_service
                -> driver конкретного типа
                    -> ModbusRtuMaster
```

Назначение компонентов:

```text
x2x_config
    чтение и проверка X2X.TXT

x2x_registry
    создание статических экземпляров модулей

x2x_catalog
    описание поддерживаемых типов и callbacks

x2x_service
    последовательный неблокирующий опрос модулей

x2x module driver
    карта регистров и state machine конкретного типа

x2x_status
    общая и специфическая диагностика
```

### 15.2 Формат X2X.TXT

Первая строка содержит количество модулей. Далее указываются числовые ID в физическом порядке цепочки.

```text
4
1
2
5
6
fin
```

Соответствие:

```text
slave 1 -> ID 1
slave 2 -> ID 2
slave 3 -> ID 5
slave 4 -> ID 6
```

Максимальное количество внешних модулей — 6.

Пустая цепочка:

```text
0
fin
```

При пустой цепочке UART0 передаётся fallback echo-test.

### 15.3 Стабильные ID

| ID | Тип |
|---:|---|
| 0 | LCP2116, запрещён в `X2X.TXT` |
| 1 | LDO1118 |
| 2 | LAI1118 |
| 3 | LDI1118 |
| 4 | LCT1114 |
| 5 | LDI1116 |
| 6 | LCT1114_2 |

> ⚠️ **Внимание**
>
> Числовой ID является частью формата `X2X.TXT`. После публикации ID нельзя изменять, удалять или повторно назначать другому типу модуля.

Конкретные карты регистров и внутренние алгоритмы модулей сопровождаются в их собственных репозиториях.

### 15.4 Runtime модуля

Общий `X2XDeviceHeader` содержит:

- type;
- ASDU;
- slave address;
- connection state;
- device fault;
- communication result;
- exception code;
- success/failed counters;
- consecutive failures;
- last update time.

Структура конкретного типа хранит прикладные данные модуля.

### 15.5 Качество и восстановление

После 5 последовательных ошибок устанавливается `connection = lost`. Последние корректные данные сохраняются, но считаются невалидными. Первый корректный ответ восстанавливает `online` и сбрасывает `consecutive_failures`.

### 15.6 Reload

```text
x2x reload
```

Если выполняется активный цикл, команда может вернуть:

```text
x2x reload: apply pending
```

Новая конфигурация применяется после безопасного завершения текущего полного цикла. Некорректный новый файл не заменяет ранее работающую конфигурацию.

### 15.7 Pause и resume

```text
x2x pause
x2x resume
```

Во время активного цикла возможен ответ:

```text
x2x polling: pause pending
```

Сервис завершает текущий цикл модуля и только затем останавливается. Это исключает обрыв UART-передачи и зависание DE в режиме передачи.

### 15.8 Сервисная проверка выходного модуля

```text
x2x ldo 1 255
```

Команда изменяет локальный выходной регистр экземпляра LDO. Значение передаётся модулю при следующем цикле опроса. Команда предназначена для аппаратной проверки и не заменяет прикладную логику управления.

[↑ К оглавлению](#contents)

---

<a id="new-x2x-module"></a>

## 16. Добавление нового X2X-модуля

Специфика нового типа сосредоточена в 3 основных местах:

1. структура данных и стабильный ID;
2. функция опроса;
3. запись в каталоге.

Новый `.cpp` также включается в проект Microchip Studio.

### 16.1 Назначить ID и описать данные

Файл:

```text
app/x2x/x2x_types.hpp
```

Пример:

```cpp
X2X_DEVICE_EXAMPLE = 7U
```

Runtime-структура:

```cpp
struct X2XExample : X2XDeviceHeader
{
    static const uint8_t SP_COUNT = 1U;
    static const uint8_t ME_COUNT = 0U;
    static const uint8_t TF_COUNT = 4U;

    uint32_t digital_inputs;
    float tf_values[TF_COUNT];
};
```

Поля связи и диагностики повторно не добавляются: они находятся в `X2XDeviceHeader`.

### 16.2 Размер статического слота

```cpp
X2X_DEVICE_SLOT_BYTES = 256U
```

`construct_device()` выполняет:

```cpp
static_assert(sizeof(DeviceType) <= X2X_DEVICE_SLOT_BYTES);
```

Если структура слишком велика, проект не соберётся. Скрытого переполнения памяти не будет.

### 16.3 Объявить драйвер

Файл:

```text
app/x2x/modules/x2x_module_drivers.hpp
```

```cpp
X2XModulePollResult x2x_module_poll_example(
    X2XDeviceHeader* device,
    X2XModuleContext& context);
```

### 16.4 Создать исходник

```text
app/x2x/modules/x2x_example.cpp
```

В качестве основы выбирается наиболее близкий существующий драйвер:

```text
x2x_ldi.cpp   дискретные входы
x2x_lai.cpp   дискретные входы и float values
x2x_ldo.cpp   запись выходов
x2x_lct.cpp   пример многошагового цикла и общего большого буфера
```

Приведённые имена используются как архитектурные примеры. Карта нового устройства определяется документацией его собственного репозитория.

### 16.5 Добавить запись в каталог

Файл:

```text
app/x2x/x2x_catalog.cpp
```

```cpp
{
    X2X_DEVICE_EXAMPLE,
    "EXAMPLE",
    sizeof(X2XExample),
    X2XExample::SP_COUNT,
    X2XExample::ME_COUNT,
    X2XExample::TF_COUNT,
    1U,
    construct_device<X2XExample, X2X_DEVICE_EXAMPLE>,
    x2x_module_poll_example,
    print_digital_inputs_and_floats<X2XExample>
}
```

Одна запись подключает тип к:

- проверке ID из `X2X.TXT`;
- статическому реестру;
- циклическому диспетчеру;
- общему статусу связи;
- печати имени и количества SP/ME/TF;
- специфическому диагностическому выводу.

Для простого дискретного модуля используется:

```cpp
print_digital_inputs<X2XExample>
```

Для DI и массива `tf_values`:

```cpp
print_digital_inputs_and_floats<X2XExample>
```

Для нестандартной структуры создаётся отдельный print callback в `x2x_catalog.cpp`.

### 16.6 Добавить исходник в проект

```xml
<Compile Include="app\x2x\modules\x2x_example.cpp">
    <SubType>compile</SubType>
</Compile>
```

### 16.7 Добавить модуль в X2X.TXT

```text
1
7
fin
```

Позиция строки определяет Modbus slave address. В данном примере адрес равен 1.

### 16.8 Обязательная проверка

1. Debug и Release собираются.
2. `x2x reload` принимает новый ID.
3. `x2x` показывает правильное имя и SP/ME/TF.
4. `success` увеличивается.
5. Физические данные соответствуют диагностическому выводу.
6. При отключении растут `failed` и `consecutive_failures`.
7. После порога появляется `connection = lost`.
8. После восстановления линия возвращается в `online`.
9. Последние корректные значения сохраняются, но учитывается quality.
10. `rtos` не показывает нежелательного роста heap или stack.

### 16.9 Что обычно не изменяется

```text
x2x_config.cpp
x2x_registry.cpp
x2x_service.cpp
x2x_status.cpp
modbus_rtu_master.cpp
```

Эти файлы изменяются при переработке общей архитектуры X2X, а не при добавлении очередной карты регистров.

[↑ К оглавлению](#contents)

---

<a id="x2x-driver"></a>

## 17. Разработка неблокирующего X2X-драйвера

### 17.1 Контекст драйвера

`X2XModuleContext` предоставляет:

```text
master
runtime
waveform
register_buffer
register_capacity
now_ms
```

Драйвер использует общий `ModbusRtuMaster` X2X-сервиса и общий временный register buffer.

> ⚠️ **Внимание**
>
> `x2x_service_poll()` уже вызывает `modbus_rtu_master_poll()` перед callback активного драйвера. Драйвер модуля не должен повторно вызывать `modbus_rtu_master_poll()`.

### 17.2 Машина состояний

```text
START_READ
    проверить master ready
    запустить запрос
    перейти в WAIT_READ
    вернуть IN_PROGRESS

WAIT_READ
    проверить result
    при BUSY вернуть IN_PROGRESS
    декодировать данные или зарегистрировать ошибку
    reset master и runtime state
    вернуть CYCLE_COMPLETE
```

### 17.3 Рабочий skeleton

```cpp
#include "x2x_module_drivers.hpp"
#include "x2x_module_common.hpp"

namespace
{
enum ExamplePollState : uint8_t
{
    EXAMPLE_POLL_START_READ = 0U,
    EXAMPLE_POLL_WAIT_READ = 1U
};

static const uint16_t EXAMPLE_REGISTER_COUNT = 9U;
static const uint16_t EXAMPLE_FLOAT_START_REGISTER = 1U;
}

X2XModulePollResult x2x_module_poll_example(
    X2XDeviceHeader* device,
    X2XModuleContext& context)
{
    if ((device == 0) ||
        (device->type != X2X_DEVICE_EXAMPLE) ||
        (context.master == 0) ||
        (context.runtime == 0) ||
        (context.register_buffer == 0) ||
        (context.register_capacity < EXAMPLE_REGISTER_COUNT))
    {
        return X2X_MODULE_POLL_CYCLE_COMPLETE;
    }

    X2XExample* example = static_cast<X2XExample*>(device);

    switch (context.runtime->state)
    {
        case EXAMPLE_POLL_START_READ:
            if (modbus_rtu_master_ready(*context.master) == 0U)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            if (modbus_rtu_master_start_read_holding(
                    *context.master,
                    device->slave_address,
                    0U,
                    EXAMPLE_REGISTER_COUNT,
                    context.register_buffer,
                    X2X_MODBUS_TRANSACTION_TIMEOUT_MS) == 0U)
            {
                x2x_module_mark_failure(
                    *device,
                    modbus_rtu_master_result(*context.master),
                    modbus_rtu_master_exception_code(*context.master));
                modbus_rtu_master_reset(*context.master);
                context.runtime->state = EXAMPLE_POLL_START_READ;
                return X2X_MODULE_POLL_CYCLE_COMPLETE;
            }

            context.runtime->state = EXAMPLE_POLL_WAIT_READ;
            return X2X_MODULE_POLL_IN_PROGRESS;

        case EXAMPLE_POLL_WAIT_READ:
        {
            const ModbusRtuResult result =
                modbus_rtu_master_result(*context.master);

            if (result == MODBUS_RTU_RESULT_BUSY)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            if (result == MODBUS_RTU_RESULT_OK)
            {
                example->digital_inputs = context.register_buffer[0];
                x2x_module_decode_float_array(
                    example->tf_values,
                    X2XExample::TF_COUNT,
                    context.register_buffer,
                    EXAMPLE_FLOAT_START_REGISTER,
                    EXAMPLE_REGISTER_COUNT);
                x2x_module_mark_success(*device, context.now_ms);
            }
            else
            {
                x2x_module_mark_failure(
                    *device,
                    result,
                    modbus_rtu_master_exception_code(*context.master));
            }

            modbus_rtu_master_reset(*context.master);
            context.runtime->state = EXAMPLE_POLL_START_READ;
            return X2X_MODULE_POLL_CYCLE_COMPLETE;
        }

        default:
            modbus_rtu_master_reset(*context.master);
            context.runtime->state = EXAMPLE_POLL_START_READ;
            return X2X_MODULE_POLL_CYCLE_COMPLETE;
    }
}
```

Адреса и размер запроса должны соответствовать фактической карте регистров модуля.

### 17.4 Порядок слов float

Штатные функции преобразования используют:

```text
первый регистр  -> младшее 16-битное слово
второй регистр -> старшее 16-битное слово
```

```cpp
x2x_module_registers_to_float(...)
x2x_module_decode_float_array(...)
```

Если новый модуль использует другой порядок слов, преобразование реализуется только в его драйвере.

> ⚠️ **Внимание**
>
> Нельзя глобально изменять общий helper ради одного нового порядка слов: это нарушит декодирование существующих модулей.

### 17.5 Фиксация результата

Успешный обмен:

```cpp
x2x_module_mark_success(*device, context.now_ms);
```

Ошибка:

```cpp
x2x_module_mark_failure(
    *device,
    modbus_rtu_master_result(*context.master),
    modbus_rtu_master_exception_code(*context.master));
```

[↑ К оглавлению](#contents)

---

<a id="new-console-command"></a>

## 18. Добавление команды служебной консоли

Основной диспетчер:

```text
app/diagnostics/diagnostic_console.cpp
```

Профильные команды рекомендуется размещать в обработчике соответствующей подсистемы:

```text
field_status_handle_command()
x2x_status_handle_command()
ethernet_status_handle_command()
rtc_status_handle_command()
watchdog_status_handle_command()
```

Порядок добавления:

1. Реализовать действие в профильном service.
2. Предоставить публичную функцию управления.
3. Добавить обработчик команды.
4. Добавить запись в соответствующую группу `help`.
5. Добавить краткий диагностический ответ.
6. Проверить корректную форму команды.
7. Проверить ошибочные аргументы.
8. Убедиться, что команда не блокирует основную задачу.

Правила вывода:

```text
name = value
name = value, next = value
0: value, 1: value
```

Использовать общие helpers:

```cpp
diagnostic_print_assignment(...)
diagnostic_print_index(...)
diagnostic_print_banner(...)
diagnostic_print_section(...)
diagnostic_print_group(...)
```

Каждая строка должна по возможности помещаться в терминал шириной около 80 символов.

[↑ К оглавлению](#contents)

---

<a id="troubleshooting"></a>

## 19. Диагностика и поиск неисправностей

### 19.1 Значения Modbus RTU result

```text
idle             запрос отсутствует
busy             транзакция выполняется
ok               получен корректный ответ
timeout          ответ не получен в установленный срок
CRC error        CRC ответа не совпадает
invalid response адрес, функция, длина или данные ответа неверны
exception        slave вернул Modbus exception
transport error  ошибка UART или незавершённый TX
invalid argument параметры запуска недопустимы
```

### 19.2 `request = busy` и `last_result = ok`

Сочетание является штатным: предыдущий запрос завершился успешно, следующий уже запущен.

### 19.3 `consecutive_failures = 255`

Счётчик насыщается на 255 и не переполняется. Значение указывает на длительное отсутствие корректных ответов.

### 19.4 Значения присутствуют, но `valid = no`

Прошивка сохраняет last-good data. Прикладное программное обеспечение обязано учитывать `valid` и `connection_lost`.

### 19.5 Modbus exception code 2

Обычно соответствует `Illegal Data Address`.

Проверить:

1. ID устройства;
2. версию firmware устройства;
3. start address;
4. register count;
5. фактическую register map.

### 19.6 Ethernet `link = down`

Проверить:

- кабель;
- коммутатор;
- питание W5500;
- выбранный ETH1/ETH2;
- `VERSIONR = 0x04`;
- socket state;
- сетевые параметры компьютера.

### 19.7 microSD file not found

Проверить:

- файл находится в корне карты;
- имя полностью совпадает;
- регистр символов;
- расширение `.TXT` или `.txt`;
- FAT16/FAT32;
- отсутствие двойного расширения;
- готовность filesystem в команде `sd`.

### 19.8 Неожиданный reset

```text
watchdog
uptime
```

Проверить reset cause, boot count и время работы после запуска.

### 19.9 Подозрение на утечку памяти

Повторить несколько одинаковых циклов:

```text
status
rtos
```

Сравнить:

- current free heap;
- minimum-ever-free heap;
- task stack high-water mark;
- linker-assigned SRAM.

Current free heap не должен уменьшаться после каждого одинакового цикла.

### 19.10 Консоль не принимает команды

Проверить:

- выбран правильный USB CDC-порт;
- команда завершается Enter;
- `periodic on` не создаёт непрерывный поток;
- контроллер не находится в цикле reset;
- uptime увеличивается;
- после переподключения открыт актуальный COM-порт.

### 19.11 HMI не возвращает кадр

Выполнить:

```text
sc16is
```

Проверить:

- HMI определён как присутствующий канал;
- обнаружена ожидаемая конфигурация SC16IS;
- тестовое устройство настроено на `9600 8N1`;
- линии RS-485 подключены с правильной полярностью;
- кадр не превышает 128 байт;
- HMI не передан другому сервису;
- SPI и chip select работают корректно.

### 19.12 X2X сначала возвращает exception, затем работает

Если после запуска наблюдается временный exception, а затем:

```text
connection = online
communication_error = ok
success увеличивается
```

необходимо проверить последовательность запуска модуля и момент готовности его register map. Единичный стартовый exception следует анализировать отдельно от устойчивого потока ошибок.

[↑ К оглавлению](#contents)

---

<a id="development-rules"></a>

## 20. Правила разработки

### 20.1 Неблокирующая работа

Не допускаются:

```text
delay в прикладном сервисе
busy-wait
while до аппаратного события
ожидание ответа без timeout
неограниченный цикл обработки входного буфера
```

Длительная операция должна иметь:

- состояние;
- функцию запуска;
- функцию `poll()`;
- timeout;
- явный код результата;
- определённый переход после ошибки.

### 20.2 Память

Рекомендуется использовать:

- статические буферы фиксированного размера;
- compile-time `static_assert`;
- проверку границ до записи;
- отсутствие динамического выделения в runtime;
- last-good data отдельно от quality;
- одного владельца большого общего буфера.

В application code не используются `new`, `delete`, `malloc` или `free`. Placement new в X2X catalog создаёт объект внутри заранее выделенного статического слота и не обращается к heap.

### 20.3 Обработка ошибок

Каждый protocol service должен:

- различать timeout, CRC, invalid response, exception и transport error;
- сохранять exception code;
- иметь порог потери связи;
- автоматически восстанавливать `online` после корректного ответа;
- не обнулять last-good data без необходимости;
- предоставлять диагностические счётчики.

### 20.4 Стиль C/C++

Настройки:

```text
.clang-format
.editorconfig
```

Основные правила:

- отступ 4 пробела;
- tab не используется;
- Allman braces;
- пробелы вокруг бинарных операторов;
- одна операция или логический шаг на строку;
- длина строки до 100 символов, когда это возможно;
- понятные имена state и result;
- Doxygen для публичных интерфейсов и точек расширения;
- final newline;
- отсутствие trailing whitespace.

### 20.5 Изменение архитектуры

Перед изменением общего protocol engine необходимо проверить, может ли задача быть решена на уровне конкретного service или драйвера.

Новый X2X-модуль обычно не требует изменений:

```text
x2x_config.cpp
x2x_registry.cpp
x2x_service.cpp
x2x_status.cpp
modbus_rtu_master.cpp
```

Новый внешний прибор не должен добавлять знание о своей модели в `ModbusRtuMaster`.

> ⚠️ **Внимание**
>
> Изменение универсального protocol layer затрагивает все сервисы, которые его используют, и требует регрессионной проверки существующих X2X-модулей и FieldSensor-портов.

[↑ К оглавлению](#contents)

---

<a id="release-checklist"></a>

## 21. Проверка перед выпуском

### 21.1 Сборка

- Debug собирается;
- Release собирается;
- отсутствуют ошибки компиляции и линковки;
- предупреждения просмотрены;
- Flash и SRAM зафиксированы;
- новые `.cpp` включены в проект;
- номер версии соответствует выпускаемой конфигурации.

### 21.2 Smoke test

```text
version
help
status
uptime
rtos
sd
field
x2x
eth
sc16is
time
watchdog
```

### 21.3 FreeRTOS

- scheduler находится в `running`;
- PLC_OK переключается с установленным периодом;
- uptime непрерывно увеличивается;
- USB CDC восстанавливается после закрытия и повторного открытия;
- heap стабилен;
- stack имеет резерв;
- watchdog обслуживается основной задачей;
- переход в SAM-BA через 1200 baud сохраняется.

### 21.4 FieldSensor

- S1–S4 обнаружены;
- параметры microSD применены;
- подключённый прибор переходит в `online`;
- `success` увеличивается;
- данные соответствуют внешнему прибору;
- отключение формирует `lost`;
- восстановление выполняется автоматически;
- last-good data сохраняются;
- quality изменяется корректно.

### 21.5 Ethernet

- ETH1 и ETH2 инициализируются;
- `VERSIONR = 0x04`;
- link поднимается;
- FC03 читает всю карту;
- `requests = responses`;
- `malformed = 0`;
- `transport_errors = 0`;
- после disconnect/reconnect сервер восстанавливается;
- сетевые параметры корректны.

### 21.6 X2X

- `X2X.TXT` загружается;
- registry создаётся;
- slave addresses и type ID соответствуют цепочке;
- подключённые модули переходят в `online`;
- данные изменяются корректно;
- timeout и exception обрабатываются;
- связь восстанавливается автоматически;
- UART errors отсутствуют;
- `pause`, `resume` и `reload` применяются на безопасной границе цикла;
- потеря одного модуля не останавливает остальные.

### 21.7 HMI

- `sc16is` определяет HMI;
- аппаратный вариант SC16IS распознаётся;
- интерфейс настроен на `9600 8N1`;
- короткий кадр возвращается без изменения;
- кадр больше FIFO возвращается несколькими шагами;
- остальные сервисы продолжают работу во время echo-test;
- сохраняется единственный владелец HMI.

### 21.8 RTC и watchdog

- `time` показывает корректное время;
- `rtc set` не блокирует другие сервисы;
- watchdog test reset перезагружает устройство;
- после reset USB CDC восстанавливается;
- reset cause определяется корректно.

### 21.9 Длительный прогон

- uptime не сбрасывается;
- heap остаётся стабильным;
- stack high-water mark стабилизируется;
- counters увеличиваются;
- отсутствуют спонтанные reset;
- transport errors не растут без внешней причины;
- консоль сохраняет работоспособность;
- Ethernet и последовательные интерфейсы работают одновременно.

> 🟦 **Информация**
>
> Для выпуска рекомендуется сохранять итоговый вывод `version`, `status`, `rtos`, `field`, `x2x`, `eth` и `sc16is` вместе с результатами сборки Release.

[↑ К оглавлению](#contents)

---

<a id="release-verification"></a>

## 22. Подтверждённое состояние версии 1.00.0

Версия 1.00.0 собрана, прошита и запущена на контроллере LCP2116.

### 22.1 Сборка Release

```text
Build succeeded
0 failed
Flash/text = 126752 bytes, 24.2%
BSS = 46248 bytes, 47.0%
```

### 22.2 RTOS и память

```text
ATSAM3X8E SRAM assigned = 51176 / 98304 bytes, 52.1%
FreeRTOS heap peak used = 10280 / 32768 bytes, 31.4%
LCP task stack peak used = 1096 / 8192 bytes, 13.4%
```

Повторные отчёты не показали роста текущего использования heap или stack.

### 22.3 microSD

Подтверждены:

- FAT32;
- `baud.TXT`;
- `Parity.TXT`;
- сетевые файлы ETH1;
- сетевые файлы ETH2;
- применение 1200 baud на S3.

### 22.4 FieldSensor

Аппаратно подтверждены реальные Modbus RTU-транзакции на S1, S2, S3 и S4:

- FC03;
- slave address 1;
- registers 0 и 1;
- потеря связи;
- `valid = no` после порога ошибок;
- сохранение last-good values;
- автоматическое восстановление;
- отсутствие UART errors.

### 22.5 Modbus TCP

Подтверждены:

- ETH1 и ETH2;
- FC03;
- публикация S1–S4 в holding registers 0..11;
- quality-регистры;
- восстановление ETH1 после link disconnect/reconnect;
- отсутствие malformed frames и transport errors;
- длительный обмен ETH1 более 14000 запросов без потери ответов.

### 22.6 X2X

Подтверждены:

- чтение `X2X.TXT`;
- работа нескольких типов модулей;
- рост `success`;
- изменение дискретных и аналоговых данных;
- корректный порядок слов `float`;
- Modbus exception path;
- timeout и lost state;
- автоматическое восстановление connection;
- отсутствие UART errors;
- стабильность памяти при работе с многошаговым драйвером и общим статическим буфером.

Специфические карты регистров и отдельные результаты функциональных модулей сопровождаются в их собственных репозиториях.

### 22.7 RTC, watchdog и console

Подтверждены:

- команда `time`;
- полный отчёт `rtc`;
- неблокирующий `rtc set`;
- аппаратный watchdog reset;
- восстановление USB после watchdog;
- сгруппированный `help`;
- разделённый `status`;
- human-readable uptime;
- проценты SRAM/heap/stack;
- единый формат диагностического вывода.

### 22.8 Итог

Технических блокеров для базового выпуска 1.00.0 не зафиксировано. Прошивка объединена в основную ветку и используется как стабильная основа платформы Lorentz.

[↑ К оглавлению](#contents)

---

<a id="quality-review"></a>

## 23. Архитектурные решения и качество кода

### 23.1 Область проверки

Проверены:

- lifecycle одной прикладной задачи FreeRTOS;
- X2X config, catalog, registry, service и драйверы;
- Modbus RTU master и минимальный slave-пример;
- FieldSensor S1–S4 и staged reload;
- общий доступ к microSD и числовые TXT-файлы;
- 2 W5500, потоковый Modbus TCP server и holding map;
- статические буферы и границы массивов;
- диагностическая консоль, RTC, watchdog и echo state machines;
- публичные заголовки и Doxygen;
- состав `LCP_Basic.cppproj` для Debug и Release.

### 23.2 Принятая структура

```text
app/       application services and runtime state
board/     LCP2116 hardware mapping
hal/       ATSAM3X8E, SC16IS7xx and W5500 access
protocol/  Modbus RTU and Modbus TCP engines
platform/  common Serial/SPI/GPIO/time facade
```

Одна FreeRTOS task последовательно вызывает неблокирующие `poll()` функции. Это упрощает владение UART/SPI и не требует дополнительных mutex, queue и стеков.

### 23.3 Общий parser конфигурации

```text
app/diagnostics/sd_config.cpp
app/diagnostics/sd_config.hpp
```

Parser поддерживает UTF-8 BOM, комментарии, пустые строки, `fin`, строгий count, проверку лишних и недостающих значений, а также numeric overflow. Динамическая память не используется.

### 23.4 Modbus RTU

- обычные slave address ограничены диапазоном 1..247;
- размеры master/slave buffers контролируются через `static_assert`;
- master и reusable slave имеют bounded timeout;
- reusable slave не может навсегда остаться в WAIT_TX.

### 23.5 Modbus TCP

Сформированный ответ сохраняется до полной постановки в TX memory W5500. Частичный ADU не передаётся.

### 23.6 RTC

`rtc set` реализован как HAL state machine и завершается через `rtc_status_poll()` без busy-wait.

### 23.7 Echo

- S1–S4 принадлежат FieldSensor и не имеют параллельного echo;
- UART0 fallback echo включается только при пустой X2X chain;
- HMI echo используется как аппаратная диагностика;
- partial TX продолжается через сохранённый offset;
- TX state имеет bounded timeout;
- SC16IS probe выполняется один раз и кэширует hardware layout.

### 23.8 FreeRTOS startup

Результат `xTaskCreate()` проверяется явно. Невосстановимые ошибки scheduler, malloc и stack overflow сведены к документированному `fatal_stop()`.

### 23.9 Разделение X2X-драйверов

Драйверы разных типов остаются отдельными, поскольку имеют собственные register maps и state machines. Общая механика вынесена в:

```text
x2x_module_common.*
x2x_catalog.*
x2x_registry.*
```

Дополнительный универсальный profile engine ухудшил бы читаемость и аппаратную проверяемость.

### 23.10 Разделение config и runtime

`FieldSensorConfig` и `FieldSensorPortState` не объединяются. Config используется для staged reload, runtime содержит master, counters, quality и timers.

### 23.11 Критерий добавления задач

Отдельные задачи для X2X, FieldSensor и Ethernet не добавлены, поскольку текущая неблокирующая модель и измеренное использование памяти не требуют этого усложнения.

[↑ К оглавлению](#contents)

---

## Статус документа

Этот `README.md` является единым руководством по базовой прошивке LCP2116 версии 1.00.0. Он объединяет эксплуатационные сведения, описание архитектуры, точки расширения, порядок диагностики и подтверждённые результаты проверки.

Изображения титульной части и дополнительные иллюстрации размещаются в каталоге:

```text
00_LCP2116/RTOS/IMG/
```

и подключаются относительными путями `IMG/...`.
