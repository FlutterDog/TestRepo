# FACT PACK

## Назначение документа

ℹ️ Этот файл хранит компактные карточки узлов по состоянию `as-built` и служит сырьём для Stage three и Stage four.

⚠️ Scope: только аппаратная часть платы и подтверждённые источниками факты.

❗ Всё спорное, конфликтное или неполное вынесено в `open_items.md`.

---

## Плата и идентичность freeze-комплекта

### Что это

Процессорный модуль семейства `Lorentz` в DIN-реечном исполнении с локальным вычислителем, двумя Ethernet-портами, сервисным USB, uSD, несколькими линиями RS-485 и внутренней линией `X2X`.

### Реализация `as-built`

* На PCB artwork присутствует маркировка `LCP2216 Rev.7.1`.
* В freeze-комплекте есть native Altium-проект, schematic PDF, PCB PDF, BOM, fabrication archive и stackup-файл.
* В `sources/extras/` присутствуют два proof/reference-документа по линейке `Lorentz` и процессорному модулю.

### Инварианты

* Основной SoT для электрической логики — native CAD bundle `sources/cad/Native.zip`.
* PDF-экспорты используются как читабельное подтверждение, но не заменяют native CAD.
* Каноническое имя текущей платы для guide — `LCP2216`, каноническая ревизия — `Rev.7.1`.

### Где смотреть

* `sources/cad/Native.zip` → `*.PrjPcb`, `*.SchDoc`, `*.pcbdoc`
* `sources/schematic/Schematic.pdf` → page `9`
* `sources/extras/Manual_Lorentz.pdf` → page `1`–`2`
* `sources/extras/Manual_LCP.pdf` → page `1`


---

## Линия питания и резервирование времени

### Что это

Входная линия питания преобразуется в локальные линии `+5V` и `+3V3`. Для часов реального времени предусмотрено резервирование от батареи `CR2032`.

### Реализация `as-built`

* На листе `PSU 24V-5V` реализован понижающий преобразователь на `U2 = TPS54331DR`.
* Выходная линия `+5V` защищена `TVS1`.
* От `+5V` сформирована линия `+3V3` через `U3 = LM1117MPX-3.3`.
* На листе процессорного модуля присутствуют батарейный держатель `B1`, компаратор `U21 = TS391ILT` и узел `BackUp battery voltage check`.
* В `sources/extras/Manual_Lorentz.pdf` модуль описан как питаемый от `+24V`, а `X2X` передаёт питание и связь между модулями.

### Инварианты

* В release можно уверенно писать про наличие локальных линий `+5V` и `+3V3`.
* В release можно уверенно писать про наличие резервирования RTC от `CR2032`.
* Для release использовать только подтверждённую архитектурную формулировку: входная линия `24V`, локальные линии `+5V` и `+3V3`; численный datasheet-range не заявлять.

### Где смотреть

* `sources/schematic/Schematic.pdf` → page `5`, title `PSU 24V-5V`
* `sources/schematic/Schematic.pdf` → page `6`, title `CPU ARM_M3`
* `sources/bom/BOM.csv` → `U2`, `U3`, `B1`, `U21`, `TVS1`
* `sources/extras/Manual_Lorentz.pdf` → page `2`, page `7`


---

## Процессорный модуль и базовая обвязка

### Что это

Основной вычислительный узел платы на микроконтроллере `ARM Cortex-M3` с JTAG, USB device/service-линии, отдельными тактовыми источниками и привязкой к периферийным интерфейсам через именованные сигналы.

### Реализация `as-built`

* `U1 = ATSAM3X8EA-AU`.
* На листе процессорного модуля присутствуют:
  * `XTL6` — `12.0 MHz`
  * `XT1` — `32.768 kHz`
  * `JTAG`
  * сигналы `CS_ETH`, `CS_ETH_2`, `CS_SD`, `SD_Detect`, `INT_CAN`, `Can Interrupt`
* На листе видны линии `D+_N` / `D-_P` к сервисному USB и линии к внешним интерфейсам.

### Инварианты

* Процессорный модуль построен вокруг одного основного `MCU`, а не вокруг CPU + внешней SDRAM/FPGA-пары.
* В release можно уверенно писать про наличие отладочных и сервисных сигналов на уровне железа.
* Конкретное описание загрузчика, прошивки и поведения по коду в scope Stage two не входит.

### Где смотреть

* `sources/schematic/Schematic.pdf` → page `6`, title `CPU ARM_M3`
* `sources/bom/BOM.csv` → `U1`, `XT1`, `XTL6`
* `sources/cad/Native.zip` → `Native/Processor.SchDoc`

---

## Ethernet-подсистема

### Что это

Два независимых Ethernet-канала на отдельных контроллерах `W5500` с раздельными разъёмами `RJ45 MagJack`.

### Реализация `as-built`

* `U11` и `U16` — `W5500`.
* `ETH1` и `ETH2` — `RJ45 MagJack`.
* Для каждого канала есть отдельный `25.0 MHz` тактовый генератор:
  * `XTL4` для одного канала
  * `XTL5` для второго канала
* На линиях PHY-части присутствуют:
  * согласующие резисторы `49.9R`
  * последовательные резисторы `33R`
  * светодиодные линии `ACTLED` / `LINKLED`

### Инварианты

* На плате физически присутствуют именно два Ethernet-порта.
* Ethernet реализован не встроенным MAC микроконтроллера, а двумя внешними специализированными контроллерами.
* Два Ethernet-порта аппаратно одинаковы; их системное назначение гибкое и может использоваться для Ethernet-панели оператора, АРМ диспетчера или клиентов `IEC 104`.

### Где смотреть

* `sources/schematic/Schematic.pdf` → page `1`, title `SPI - Ethernet`, file token `ETH_HMI.SchDoc`
* `sources/schematic/Schematic.pdf` → page `2`, title `SPI - Ethernet`, file token `Ethernet.SchDoc`
* `sources/bom/BOM.csv` → `U11, U16`, `ETH1, ETH2`, `XTL4, XTL5`
* `sources/extras/Manual_Lorentz.pdf` → page `2`


---

## RS-485 и внутренняя линия `X2X`

### Что это

Многоканальная подсистема последовательной связи на физическом уровне `RS-485` с выделенными линиями для внешних устройств, панели оператора, ПК и линии `X2X` для модулей.

### Реализация `as-built`

* В BOM присутствуют `7` трансиверов `SN65HVD3082EDR` (`U4…U10`).
* На schematic PDF явно видны шесть дифференциальных линий `RS1_P/N`…`RS6_P/N`.
* По operator review их functional mapping для release принимать так:
  * `RS1`…`RS4` — линии для внешних устройств
  * `RS5` — выделенная линия `HMI`
  * `RS6` — выделенная линия `PC`
* На листе `PLUGS` и на page `7` присутствует отдельная линия `X_L / X_H` для `X2X`.
* На листе `SPI - RS485` присутствует `U18 = SC16IS752IPW,128` и кварц `XTL2 = 14.7456 MHz`.

### Инварианты

* Для release принимать формулировку: на физическом уровне `RS-485` реализовано семь линий — `RS1`…`RS4`, `HMI`, `PC` и `X2X`.
* Только `RS1`…`RS4` описывать как линии для внешних устройств общего назначения.
* На линиях `RS-485` присутствуют элементы защиты и ограничения тока.

### Где смотреть

* `sources/schematic/Schematic.pdf` → page `4`, title `PLUGS`
* `sources/schematic/Schematic.pdf` → page `7`, title `SPI - RS485`
* `sources/bom/BOM.csv` → `U4…U10`, `U18`, `XTL2`, `F1…F15`, `TVS1…TVS22`
* `sources/extras/Manual_Lorentz.pdf` → page `2`, page `3`, page `7`


---

## Сервисный USB

### Что это

USB-интерфейс сервисного и диагностического назначения.

### Реализация `as-built`

* Разъём `J1` — `USB B Socket`.
* На листе `PLUGS` узел подписан как `USB - Firmware/Debug`.
* На листе процессорного модуля линии `D-_N` и `D+_P` заведены к `U1`.
* На этих линиях присутствуют элементы `Z1`, `Z2` с BOM-описанием `Chip Guard 0603 5 Volts`.

### Инварианты

* В release можно уверенно писать про наличие сервисного USB-порта.
* Утверждения про режимы работы USB на уровне прошивки в scope Stage two не входят.

### Где смотреть

* `sources/schematic/Schematic.pdf` → page `4`, title `PLUGS`
* `sources/schematic/Schematic.pdf` → page `6`, title `CPU ARM_M3`
* `sources/bom/BOM.csv` → `J1`, `Z1`, `Z2`
* `sources/extras/Manual_Lorentz.pdf` → page `2`, page `6`

---

## uSD и хранение данных

### Что это

Слот microSD, подключённый по SPI и дополненный сигналом детекта карты.

### Реализация `as-built`

* `SD1` — `uSD card holder`.
* На листе `SPI - MicroSD` присутствуют сигналы:
  * `MISO`
  * `MOSI`
  * `SPCK`
  * `CS_SD`
  * `SD_Detect`
* На листе процессорного модуля эти сигналы заведены к `U1`.
* На PCB artwork слот `uSD` присутствует на верхней стороне платы рядом с `ETH1`.

### Инварианты

* В release можно уверенно писать про внешний съёмный microSD-носитель.
* В release нельзя без отдельного системного источника писать про формат файлов, структуру каталогов и поведение загрузчика.

### Где смотреть

* `sources/schematic/Schematic.pdf` → page `8`, title `SPI - MicroSD`
* `sources/schematic/Schematic.pdf` → page `6`, title `CPU ARM_M3`
* `sources/schematic/Schematic.pdf` → page `9`
* `sources/bom/BOM.csv` → `SD1`
* `sources/extras/Manual_Lorentz.pdf` → page `2`

---

## Световая индикация

### Что это

Локальная световая индикация состояния модуля и интерфейсов.

### Реализация `as-built`

* На листе `LED Indication` присутствуют `LED1…LED10`.
* В BOM присутствуют:
  * `4` зелёных светодиода
  * `6` жёлтых светодиодов
  * `5` light guide (`LG1…LG5`)
* Коммутация индикаторов выполнена через `T1…T7 = BSS138`.
* На листе присутствуют сигналы `PLC_OK`, `SD_OK`, `X2X_Led`, `RTS4`, `RTS5`, `RTS6`, `UART_RTS_2`, `UART_RTS_3`, `RTS_PC`.

### Инварианты

* Подсистема индикации дискретная и привязана к именованным аппаратным сигналам.
* В release можно описывать физические индикаторы и их группировку, но не программную логику их мигания без дополнительного источника.

### Где смотреть

* `sources/schematic/Schematic.pdf` → page `3`, title `LED Indication`
* `sources/bom/BOM.csv` → `LED1…LED10`, `LG1…LG5`, `T1…T7`

---

## Защита линий и ограничение аварийных воздействий

### Что это

Набор аппаратных средств защиты линий питания и коммуникации: TVS-диоды, полимерные предохранители и USB line-protection.

### Реализация `as-built`

* В BOM присутствуют `22` TVS-диода `SMAJ5.0CA`.
* В BOM присутствуют:
  * `14` PPTC `50 mA`
  * `1` PPTC `300 mA`
* На листе `SPI - RS485` у внешних линий видны PPTC-предохранители `F1…F15` и TVS-узлы `TVS2…TVS22`.
* На листе `PSU 24V-5V` выходная линия `+5V` имеет `TVS1`.
* На сервисном USB присутствуют `Z1`, `Z2`.

### Инварианты

* Факт наличия аппаратной защиты линий подтверждён сразу несколькими источниками.
* Подтверждена защита линий `RS-485` через TVS и PPTC; в release использовать именно эту формулировку.

### Где смотреть

* `sources/schematic/Schematic.pdf` → page `5`
* `sources/schematic/Schematic.pdf` → page `6`
* `sources/schematic/Schematic.pdf` → page `7`
* `sources/bom/BOM.csv` → `TVS1…TVS22`, `F1…F15`, `Z1`, `Z2`
* `sources/cad/Native.zip` → `Native/OvervoltageProtection.SchDoc`


---

## Коннекторы и внешние точки доступа

### Что это

Набор физических интерфейсных точек для обслуживания, Ethernet, полевых линий и внутренней шины модулей.

### Реализация `as-built`

* На PCB artwork и schematic PDF видны:
  * `ETH1`
  * `ETH2`
  * `J1 USB`
  * `SD1`
  * `Btn1 Reset`
  * `Btn2 Erase`
  * `P1` для блока `RS485 Lines`
  * `P3` для `CAN Bus X2X`
* На PCB artwork видны также батарейный держатель `B1` и локальные test points `TP1…TP5`.

### Инварианты

* Плата содержит как пользовательские внешние интерфейсы, так и сервисные точки обслуживания.
* Кнопки `Reset` и `Erase` физически присутствуют на плате.

### Где смотреть

* `sources/schematic/Schematic.pdf` → page `4`
* `sources/schematic/Schematic.pdf` → page `6`
* `sources/schematic/Schematic.pdf` → page `9`
* `sources/pcb/Job1.PDF` → page `1`–`2`
* `sources/bom/BOM.csv` → `Btn1`, `Btn2`, `J1`, `ETH1`, `ETH2`, `P1`, `P3`, `SD1`, `B1`

---

## PCB, stackup и fabrication-факты

### Что это

Шестислойная плата с отдельным stackup-файлом и fabrication archive.

### Реализация `as-built`

* В `LCP.stackup` заданы проводящие слои:
  * `Top`
  * `Gnd`
  * `Power`
  * `HighSpeed`
  * `High speed`
  * `Bottom`
* Для каждого проводящего слоя в stackup задана толщина меди `0.018 mm`.
* В stackup заданы диэлектрики:
  * `0.127 mm`
  * `0.127 mm`
  * `0.864 mm`
  * `0.127 mm`
* Сверху и снизу присутствует solder resist по `0.01016 mm`.
* Gerber report подтверждает выпуск artwork-файлов для `Top`, `Gnd`, `Power`, двух внутренних высокоскоростных слоёв и `Bottom`.
* Drill report подтверждает `374` plated holes с диапазоном инструмента от `0.2 mm` до `3.3 mm`.

### Инварианты

* В release можно уверенно писать, что плата шестислойная.
* В release можно уверенно писать про наличие отдельных силового и земляного внутренних слоёв.
* Расчёт импеданса и manufacturing-ограничения можно упоминать только в той части, которая подтверждается stackup-файлом и export-отчётами, без догадки о fab-tune.

### Где смотреть

* `sources/stackup/LCP.stackup`
* `sources/gerber/Gerber.zip` → `.REP`, `.DRR`, `Status Report.Txt`
* `sources/pcb/Job1.PDF` → page `1`–`2`
* `sources/schematic/Schematic.pdf` → page `9`


---

## Что уже достаточно закрыто для Stage three

### Можно считать закрытым как фактический слой

* наличие native CAD, schematic PDF, PCB PDF, BOM, Gerber и stackup
* каноническое имя платы `LCP2216` и ревизия `Rev.7.1`
* origin page `9` как assembly view / PCB artwork proof
* базовая архитектура линий питания `+5V` и `+3V3`
* основной вычислительный узел на `ATSAM3X8EA-AU`
* два Ethernet-канала на `W5500`
* microSD по SPI
* сервисный USB
* линии `RS1`…`RS4` для внешних устройств
* отдельные линии `HMI` и `PC` на физическом уровне `RS-485`
* отдельная линия `X2X`
* дискретная световая индикация
* шестислойная плата и базовые fabrication-факты

### Что исключить из release-утверждений на следующих стадиях

* утверждение о гальванической изоляции интерфейсов
* точный численный диапазон входной линии питания
