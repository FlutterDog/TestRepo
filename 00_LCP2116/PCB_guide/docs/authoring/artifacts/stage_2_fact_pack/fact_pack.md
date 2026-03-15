# FACT PACK

## Назначение документа

ℹ️ Этот файл фиксирует фактическую базу Stage 2 для платы процессорного модуля LCP из freeze-комплекта.

ℹ️ Здесь разделены четыре класса утверждений:

- implemented fact;
- component capability fact;
- board-supported capability;
- design-intent oriented interpretation.

❗ Этот файл не является release-текстом и не должен превращаться в готовую пояснительную записку.

---

## Source sufficiency check

### Что уже есть

- native Altium archive в `sources/cad/Native.zip`;
- schematic export в `sources/schematic/Schematic.pdf`;
- PCB export в `sources/pcb/Job1.PDF`;
- BOM export в `sources/bom/BOM.csv`;
- fabrication archive в `sources/gerber/Gerber.zip`;
- stackup file в `sources/stackup/LCP.stackup`;
- product manuals в `sources/extras/Manual_LCP.pdf` и `sources/extras/Manual_Lorentz.pdf`.

### Чего не хватает

⚠️ `sources/extras/references/` пустая.

⚠️ Для более сильного component-capability слоя не хватает официальных component datasheet / reference manual по ключевым ИС:

- `ATSAM3X8E`;
- `W5500`;
- `SN65HVD3082E`;
- `SC16IS752`;
- `TPS54331`;
- `LM1117-3.3`.

### Влияние на Stage three / Stage four

ℹ️ Текущей factual base достаточно для перехода на Stage three.

⚠️ Без official references Stage four можно писать по архитектуре платы и product-level claims, но нельзя уверенно насыщать текст детальными chip-level режимами, лимитами и функциональными обещаниями сверх того, что подтверждено BOM и manuals.

### Style exemplars

ℹ️ В `docs/authoring/pipeline/style_references/` уже лежит `Ti Style.pdf`.

ℹ️ Для Stage three этого достаточно как минимум для выбора общей композиции, плотности текста и figure strategy.

---

## Identity and scope

### Board family

ℹ️ FACT: freeze-комплект относится к процессорному модулю семейства `Lorentz`.

Подтверждение:

- `sources/extras/Manual_LCP.pdf`, титул: модульные свободно программируемые контроллеры `Lorentz`;
- `sources/extras/Manual_Lorentz.pdf`, титул: процессорный модуль `LCP2116`.

### Current identity conflict

⚠️ ASSUMPTION: freeze-комплект описывает одно и то же изделие, но внутри него присутствует конфликт по маркировке и ревизии.

Наблюдаемые токены:

- `LCP2116` — manuals и часть native metadata;
- `LCP2216` — строки внутри `sources/cad/Native.zip` / `*.pcbdoc`;
- `Rev.7.1` — строки внутри `*.pcbdoc`;
- `LCP rev 7.2` — строки внутри `*.BomDoc`, `*.OutJob`, `*.Dat`, `*.PcbLib`.

❗ До закрытия конфликта нельзя делать жёсткое release-утверждение, что весь комплект строго относится к одной единственной ревизии без наследованных артефактов.

### Scope boundary

ℹ️ FACT: Stage 2 покрывает только hardware-level архитектуру платы и module-level claims, подтверждённые freeze-источниками.

⚠️ Firmware behavior, protocol implementation details, watchdog logic, file formats и UI-логика не входят в factual scope текущей стадии.

---

## Implemented facts

### Подсистема процессора

ℹ️ FACT: в BOM установлен микроконтроллер `ATSAM3X8EA-AU`.

Подтверждение:

- `sources/bom/BOM.csv` -> `U1` -> `ATSAM3X8EA-AU` -> `ARM M3 32bit 84 MHz`.

ℹ️ FACT: в native CAD есть отдельный schematic sheet процессорной части.

Подтверждение:

- `sources/cad/Native.zip` -> `Native/Processor.SchDoc`.

ℹ️ FACT: product manual описывает модуль как решение на базе `ARM Cortex M3` с частотой `84 MHz`.

Подтверждение:

- `sources/extras/Manual_Lorentz.pdf` -> раздел `Общая информация` и таблица технических характеристик.

### Подсистема Ethernet

ℹ️ FACT: на плате реализованы два Ethernet-контроллера `W5500`.

Подтверждение:

- `sources/bom/BOM.csv` -> `U11, U16` -> `W5500` -> `Ethernet ICs 3in1 Enet Controller TCP/IP +MAC+PHY`.

ℹ️ FACT: на плате установлены два RJ45 MagJack-разъёма.

Подтверждение:

- `sources/bom/BOM.csv` -> `ETH1, ETH2` -> `J0011D21BNL` -> `RJ45 MagJack`.

ℹ️ FACT: в native CAD Ethernet разнесён минимум на два sheet-level домена.

Подтверждение:

- `sources/cad/Native.zip` -> `Native/Ethernet.SchDoc`;
- `sources/cad/Native.zip` -> `Native/ETH_HMI.SchDoc`.

ℹ️ FACT: product manual заявляет два Ethernet-интерфейса `E1` и `E2` со скоростью `10/100BaseT` и разъёмами `RJ45`.

Подтверждение:

- `sources/extras/Manual_Lorentz.pdf` -> раздел `Общая информация`;
- `sources/extras/Manual_Lorentz.pdf` -> раздел `Интерфейсы`.

### Подсистема RS-485 и последовательных интерфейсов

ℹ️ FACT: BOM содержит семь RS-485 transceiver `SN65HVD3082EDR`.

Подтверждение:

- `sources/bom/BOM.csv` -> `U4, U5, U6, U7, U8, U9, U10` -> `SN65HVD3082EDR`.

ℹ️ FACT: в native CAD есть отдельный sheet `RS485`.

Подтверждение:

- `sources/cad/Native.zip` -> `Native/RS485.SchDoc`.

ℹ️ FACT: manual описывает модуль как имеющий несколько RS-485 портов и отдельную внутреннюю шину `X2X`.

Подтверждение:

- `sources/extras/Manual_Lorentz.pdf` -> раздел `Общая информация`;
- `sources/extras/Manual_Lorentz.pdf` -> раздел `Интерфейсы`.

ℹ️ FACT: BOM содержит отдельный интерфейсный мост `SC16IS752`.

Подтверждение:

- `sources/bom/BOM.csv` -> `U18` -> `SC16IS752IPW,128` -> `UART Interface IC I2C/SPI`.

⚠️ ASSUMPTION: `SC16IS752` используется как часть расширения / обслуживания последовательных каналов.

Причина:

- это инженерно правдоподобно по типу микросхемы и общей архитектуре модуля;
- но без component datasheet и без чтения native schematic connections нельзя фиксировать точную роль как FACT.

### Подсистема питания

ℹ️ FACT: BOM содержит входной step-down regulator `TPS54331DR`.

Подтверждение:

- `sources/bom/BOM.csv` -> `U2` -> `TPS54331DR` -> `3A 28V In Step Down`.

ℹ️ FACT: BOM содержит линейный стабилизатор `LM1117MPX-3.3`.

Подтверждение:

- `sources/bom/BOM.csv` -> `U3` -> `LM1117MPX-3.3` -> `800MA 3.3V`.

ℹ️ FACT: в native CAD есть отдельный sheet питания.

Подтверждение:

- `sources/cad/Native.zip` -> `Native/Power Supply.SchDoc`.

ℹ️ FACT: product manual задаёт питание модуля от линии `24 V` постоянного тока и указывает защиту от неправильной полярности.

Подтверждение:

- `sources/extras/Manual_Lorentz.pdf` -> таблица технических характеристик.

### Подсистема памяти и носителей

ℹ️ FACT: BOM содержит держатель `uSD` карты.

Подтверждение:

- `sources/bom/BOM.csv` -> `SD1` -> `DM3AT-SF-PEJM5` -> `uSD card holder`.

ℹ️ FACT: в native CAD есть отдельный sheet для узла карты памяти.

Подтверждение:

- `sources/cad/Native.zip` -> `Native/SD Holder.SchDoc`.

ℹ️ FACT: manual заявляет хранение конфигурации и журналов на `uSD` карте.

Подтверждение:

- `sources/extras/Manual_Lorentz.pdf` -> раздел `Общая информация`;
- `sources/extras/Manual_Lorentz.pdf` -> таблица технических характеристик.

### RTC и резервирование времени

ℹ️ FACT: BOM содержит держатель батареи `CR2032`.

Подтверждение:

- `sources/bom/BOM.csv` -> `B1` -> `Battery Holder CR2032`.

ℹ️ FACT: manual заявляет резервирование часов реального времени от батареи `CR2032`.

Подтверждение:

- `sources/extras/Manual_Lorentz.pdf` -> раздел `Общая информация`.

⚠️ ASSUMPTION: конкретная RTC-микросхема не идентифицирована по текущему BOM-scan.

Причина:

- в доступном quick-pass не найден отдельный очевидный RTC part number;
- возможно, функция реализована во внутреннем домене процессора или через менее очевидный компонент.

### Подсистема защиты

ℹ️ FACT: на плате присутствует развитый набор TVS-элементов.

Подтверждение:

- `sources/bom/BOM.csv` -> `TVS1 ... TVS22` -> `SMAJ5.0CA` -> `ESD Protection Diodes 5V`.

ℹ️ FACT: в BOM присутствует набор самовосстанавливающихся предохранителей.

Подтверждение:

- `sources/bom/BOM.csv` -> `F1, F3 ... F15` -> `PPTC 30V 50MA`;
- `sources/bom/BOM.csv` -> `F2` -> `PPTC 30V 300MA`.

ℹ️ FACT: manual заявляет интегрированный самовосстанавливающийся предохранитель и защиту от неверной полярности.

Подтверждение:

- `sources/extras/Manual_Lorentz.pdf` -> таблица технических характеристик.

### Подсистема индикации и локального обслуживания

ℹ️ FACT: native CAD содержит отдельный sheet `LEDs`.

Подтверждение:

- `sources/cad/Native.zip` -> `Native/LEDs.SchDoc`.

ℹ️ FACT: BOM содержит два тактовых переключателя.

Подтверждение:

- `sources/bom/BOM.csv` -> `Btn1, Btn2` -> `Tact Switch`.

ℹ️ FACT: BOM содержит разъём `USB Type B`.

Подтверждение:

- `sources/bom/BOM.csv` -> `J1` -> `USBB-1J`.

ℹ️ FACT: manual относит `USB 2.0` к диагностике и техническому обслуживанию.

Подтверждение:

- `sources/extras/Manual_Lorentz.pdf` -> раздел `Общая информация` и раздел `Интерфейсы`.

### PCB and stackup

ℹ️ FACT: в stackup-файле читается симметричный многослойный стек с внешними слоями `Top` и `Bottom` и внутренними медными слоями `Gnd`, `Power`, `HighSpeed`, `High speed`.

Подтверждение:

- `sources/stackup/LCP.stackup`.

ℹ️ FACT: по названиям медных слоёв видна ориентация проекта на разделение power / ground domains и отдельные high-speed routing layers.

Подтверждение:

- `sources/stackup/LCP.stackup` -> имена слоёв.

⚠️ ASSUMPTION: для release-текста безопасно говорить о шестислойной плате.

Причина:

- стек содержит шесть медных слоёв: `Top`, `Gnd`, `Power`, `HighSpeed`, `High speed`, `Bottom`;
- но без отдельного fabrication drawing лучше не углубляться в manufacturing claims сверх структуры stackup.

---

## Component capability facts

### Из product manual

ℹ️ FACT: модуль описан как центральный процессорный модуль с поддержкой:

- `RS485`;
- `Ethernet`;
- `USB`;
- `X2X`;
- `uSD` для конфигурации и журналов.

Подтверждение:

- `sources/extras/Manual_Lorentz.pdf` -> раздел `Общая информация`.

ℹ️ FACT: manual указывает следующие product-level характеристики:

- `ARM`, `84 MHz`;
- `SRAM 100 kB`;
- `RAM buffer 4 kB ECC`;
- `поле переменных 512 kB FRAM`;
- `минимальное время цикла 2 ms`;
- `память приложений 2 GB microSD`.

Подтверждение:

- `sources/extras/Manual_Lorentz.pdf` -> таблица технических характеристик.

ℹ️ FACT: manual указывает для Ethernet физический уровень `10BASE-T/100BASE-TX`.

Подтверждение:

- `sources/extras/Manual_Lorentz.pdf` -> раздел `Интерфейсы`.

ℹ️ FACT: manual указывает разные режимы для интерфейсов `RS485`:

- интерфейс `0` до `1.0 МБит/с`;
- интерфейсы `1 ... 6` до `110 кБит/с`.

Подтверждение:

- `sources/extras/Manual_Lorentz.pdf` -> раздел `Интерфейсы`.

### Из BOM-level component descriptions

ℹ️ FACT: BOM-поле описания уже даёт безопасный минимальный capability-layer по нескольким компонентам:

- `W5500` -> контроллер с `TCP/IP + MAC + PHY`;
- `TPS54331DR` -> step-down regulator до `28 V` входа;
- `LM1117MPX-3.3` -> `3.3 V` LDO;
- `SN65HVD3082EDR` -> low-power `RS-485 interface IC`.

Подтверждение:

- `sources/bom/BOM.csv`.

⚠️ ASSUMPTION: без official datasheets эти BOM-descriptions нельзя автоматически разворачивать в полный перечень режимов, таймингов и electrical limits.

---

## Board-supported capabilities

### Что можно утверждать уверенно

ℹ️ FACT: плата в текущем freeze-комплекте поддерживает как минимум следующие board-level возможности:

- центральный вычислительный узел на базе `ATSAM3X8E`-семейства;
- два Ethernet-порта с отдельными RJ45 MagJack;
- несколько изолированных / развязанных последовательных интерфейсов домена `RS-485`;
- подключение `uSD` карты;
- `USB Type B` для сервисного доступа;
- локальную индикацию и кнопки;
- питание от `24 V` с локальной генерацией низковольтных rails;
- резервирование часов от батареи `CR2032`.

Подтверждение:

- `sources/bom/BOM.csv`;
- `sources/cad/Native.zip`;
- `sources/extras/Manual_Lorentz.pdf`.

### Что можно утверждать только осторожно

⚠️ ASSUMPTION: плата поддерживает семь физических каналов `RS-485` на уровне transceiver count.

Основание:

- в BOM семь `SN65HVD3082EDR`;
- manual в одном месте говорит о портах `1...7`, а в другой таблице суммарно показывает `6x RS485 + 1x X2X`.

⚠️ ASSUMPTION: один из последовательных каналов может быть логически выделен под `X2X` и поэтому не должен механически суммироваться как внешний пользовательский `RS-485` порт.

Основание:

- manual отделяет `X2X` от обычных интерфейсов ввода-вывода;
- но hardware-level transceiver count сам по себе не говорит, как именно линия отдана в изделии пользователю.

### Что нельзя обещать без усиления source base

❗ Нельзя без official datasheets и native connectivity review обещать:

- полный стек protocol-level features Ethernet controllers;
- точные режимы и лимиты каждого `RS-485` канала по отдельности;
- точный объём и реализацию `FRAM` как board-level implementation fact;
- galvanic isolation topology каждого интерфейса по компонентному уровню;
- точные EMC / surge performance claims.

---

## Draft-useful subsystem summaries

### Power

Плата рассчитана на входной промышленный домен `24 V`, после чего формирует локальные низковольтные rails через комбинацию buck-преобразователя и `3.3 V` LDO. По BOM и manual видно, что питание дополнено защитой от переполюсовки, самовосстанавливающимися предохранителями и распределённой TVS-защитой.

### Processor core

В центре платы находится вычислительный узел на базе `ATSAM3X8E`-семейства, а product-level manual позиционирует модуль как центральный CPU блок линейки `Lorentz`. Это уже даёт основу для будущего текста про роль платы как coordinator / communication hub, но не требует уходить в firmware detail.

### Communication fabric

Коммуникационная часть построена вокруг двух Ethernet-контроллеров, группы `RS-485` transceiver, сервисного `USB` и внутренней шины `X2X`. Это позволяет описывать модуль как коммуникационно-насыщенный контроллер для связи с внешними устройствами, HMI и локальными / удалёнными I/O.

### Data retention and logging

Наличие `uSD` и батарейного резервирования времени подтверждает, что плата проектировалась не как «голая CPU board», а как законченный контроллерный модуль с журналированием, сохранением конфигурации и удержанием времени при отключении питания.

---

## Stage two conclusion

### Достаточность factual base

ℹ️ Текущей factual base достаточно для перехода на Stage three.

### Что нужно догрузить для сильного Stage four

⚠️ Полезно догрузить в `sources/extras/references/`:

- official datasheet / reference manual на `ATSAM3X8E`;
- official datasheet на `W5500`;
- official datasheet на `SN65HVD3082E`;
- official datasheet на `SC16IS752`;
- official datasheet на `TPS54331`;
- official datasheet на `LM1117-3.3`.

### Что уже достаточно хорошо обеспечено

ℹ️ Для board architecture, subsystem decomposition и outline planning текущего комплекта достаточно.
