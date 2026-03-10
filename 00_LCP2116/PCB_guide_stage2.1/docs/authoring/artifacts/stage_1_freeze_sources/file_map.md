# FILE MAP

## Document role

ℹ️ Этот файл — канонический слой трассируемости между инженерными утверждениями и Source of Truth.

❗ Одна запись = одно проверяемое утверждение.

---

## Status legend

ℹ️ FACT — подтверждено источником.

⚠️ ASSUMPTION — допущение, явно отмеченное и контролируемое.

❗ UNKNOWN — пока не подтверждено; требуется источник или отдельная проверка.

---

## Entries

### Statement ID

* `FM-001`

### Status

* `FACT`

### Statement

* freeze-комплект содержит native Altium-проект платы, а не только read-only exports

### Source anchor

* File: `sources/cad/Native.zip`
* Anchor: `Native/МЦТР.00010572.00.001.PrjPcb`, `Native/МЦТР.00010572.00.001.pcbdoc`, `Native/МЦТР.00010572.00.001.PcbLib`

### Artifact group

* `other`

### Notes / conflicts / follow-up

* native CAD принят как главный editable source

---

### Statement ID

* `FM-002`

### Status

* `FACT`

### Statement

* native CAD bundle включает отдельный schematic-sheet для питания платы

### Source anchor

* File: `sources/cad/Native.zip`
* Anchor: `Native/Power Supply.SchDoc`

### Artifact group

* `power`

### Notes / conflicts / follow-up

* sheet-level decomposition подтверждена по составу архива

---

### Statement ID

* `FM-003`

### Status

* `FACT`

### Statement

* native CAD bundle включает отдельный schematic-sheet для процессорной части

### Source anchor

* File: `sources/cad/Native.zip`
* Anchor: `Native/Processor.SchDoc`

### Artifact group

* `processor`

### Notes / conflicts / follow-up

* детализация по MCU/CPU подтверждена на Stage two по schematic PDF и BOM

---

### Statement ID

* `FM-004`

### Status

* `FACT`

### Statement

* native CAD bundle включает отдельные schematic sheets для Ethernet и ETH_HMI

### Source anchor

* File: `sources/cad/Native.zip`
* Anchor: `Native/Ethernet.SchDoc`, `Native/ETH_HMI.SchDoc`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* назначение каждого порта как `HMI` или иное требует отдельного закрытия

---

### Statement ID

* `FM-005`

### Status

* `FACT`

### Statement

* native CAD bundle включает отдельные schematic sheets для RS485 и CAN интерфейсов

### Source anchor

* File: `sources/cad/Native.zip`
* Anchor: `Native/RS485.SchDoc`, `Native/CAN.SchDoc`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* это подтверждает наличие нескольких последовательных/полевых интерфейсных доменов в структуре проекта

---

### Statement ID

* `FM-006`

### Status

* `FACT`

### Statement

* native CAD bundle включает отдельный schematic-sheet для узла защиты от перенапряжения

### Source anchor

* File: `sources/cad/Native.zip`
* Anchor: `Native/OvervoltageProtection.SchDoc`

### Artifact group

* `protection`

### Notes / conflicts / follow-up

* сам лист отсутствует в current schematic PDF export

---

### Statement ID

* `FM-007`

### Status

* `FACT`

### Statement

* native CAD bundle включает отдельные sheets для подключений и носителя данных

### Source anchor

* File: `sources/cad/Native.zip`
* Anchor: `Native/Plugs.SchDoc`, `Native/SD Holder.SchDoc`

### Artifact group

* `connectors`

### Notes / conflicts / follow-up

* sheet-level факт подтверждён и читабельно виден в schematic PDF

---

### Statement ID

* `FM-008`

### Status

* `FACT`

### Statement

* в accepted bundle присутствует read-only schematic export в PDF-формате

### Source anchor

* File: `sources/schematic/Schematic.pdf`
* Anchor: `pdfinfo -> Pages: 9`

### Artifact group

* `other`

### Notes / conflicts / follow-up

* PDF пригоден для чтения и ревью, но не заменяет native CAD

---

### Statement ID

* `FM-009`

### Status

* `FACT`

### Statement

* активный набор schematic sheets для текущего guide соответствует `Schematic.pdf`; ранее выявленное расхождение было связано с случайно загруженными лишними native files

### Source anchor

* File: `sources/schematic/Schematic.pdf`
* Anchor: `operator clarification -> extra native files removed; active sheet set now matches schematic PDF`

### Artifact group

* `other`

### Notes / conflicts / follow-up

* для следующего snapshot parity-check считать закрытым по operator review
---

### Statement ID

* `FM-010`

### Status

* `FACT`

### Statement

* accepted bundle содержит отдельный PCB/layout PDF export

### Source anchor

* File: `sources/pcb/Job1.PDF`
* Anchor: `pdfinfo -> Pages: 7`

### Artifact group

* `mechanical`

### Notes / conflicts / follow-up

* имя `Job1.PDF` обобщённое, но файл принят как canonical PCB export текущего freeze

---

### Statement ID

* `FM-011`

### Status

* `FACT`

### Statement

* native CAD bundle содержит output-job файлы, связанные с генерацией производственных и документных exports

### Source anchor

* File: `sources/cad/Native.zip`
* Anchor: `Native/Job1.OutJob`, `Native/МЦТР.00010572.00.001.OutJob`

### Artifact group

* `other`

### Notes / conflicts / follow-up

* связь между `Job1.OutJob` и `sources/pcb/Job1.PDF` вероятна по имени, но формально не доказана

---

### Statement ID

* `FM-012`

### Status

* `FACT`

### Statement

* freeze-комплект содержит канонический BOM export с полями designator, footprint, quantity, part description и part number

### Source anchor

* File: `sources/bom/BOM.csv`
* Anchor: `header row -> Comment;Designator;Footprint;Quantity;Part Description;Part Number`

### Artifact group

* `bom`

### Notes / conflicts / follow-up

* BOM пригоден для подтверждения component-level фактов

---

### Statement ID

* `FM-013`

### Status

* `FACT`

### Statement

* fabrication archive содержит drill/report файлы и набор artwork-файлов для верхних, внутренних и нижнего проводящих слоёв

### Source anchor

* File: `sources/gerber/Gerber.zip`
* Anchor: `.DRR`, `-Plated.TXT`, `.GTL`, `.GBL`, `.GTS`, `.GTO`, `.GBS`, `.G1`, `.G2`, `.GP1`, `.GP2`, `Status Report.Txt`

### Artifact group

* `mechanical`

### Notes / conflicts / follow-up

* gerber archive пригоден для fabrication-level подтверждения stackup и drill

---

### Statement ID

* `FM-014`

### Status

* `FACT`

### Statement

* в freeze-комплекте присутствует отдельный stackup-файл с именованными слоями

### Source anchor

* File: `sources/stackup/LCP.stackup`
* Anchor: `LAYER_V8_3NAME=Top`, `LAYER_V8_5NAME=Gnd`, `LAYER_V8_7NAME=Power`, `LAYER_V8_9NAME=HighSpeed`, `LAYER_V8_11NAME=High speed`, `LAYER_V8_13NAME=Bottom`

### Artifact group

* `stackup`

### Notes / conflicts / follow-up

* Stage two подтвердил шестислойную проводящую структуру

---

### Statement ID

* `FM-015`

### Status

* `FACT`

### Statement

* идентичность ревизии опирается на silkscreen `LCP2216 Rev.7.1` на PCB artwork и operator-approved Board ID `LCP_rev7.1`

### Source anchor

* File: `sources/schematic/Schematic.pdf`
* Anchor: `page 9 -> LCP2216 Rev.7.1`; `operator clarification -> canonical revision Rev.7.1`

### Artifact group

* `other`

### Notes / conflicts / follow-up

* токены внутри export-path не считаются более сильным source, чем silkscreen и operator resolution
---

### Statement ID

* `FM-016`

### Status

* `FACT`

### Statement

* на PCB artwork присутствует silkscreen-маркировка `LCP2216 Rev.7.1`

### Source anchor

* File: `sources/schematic/Schematic.pdf`
* Anchor: `page 9 -> text LCP2216 Rev.7.1`

### Artifact group

* `mechanical`

### Notes / conflicts / follow-up

* page `9` трактовать как assembly view / PCB artwork proof
---

### Statement ID

* `FM-017`

### Status

* `FACT`

### Statement

* каноническое имя текущей платы для guide — `LCP2216`; токен `LCP2116` в reference-manual относится к аналогичному модулю другой формы исполнения

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/extras/Manual_Lorentz.pdf`
* Anchor: `page 9 -> LCP2216 Rev.7.1`; `manual page 1 -> Процессорный модуль LCP2116`; `operator clarification -> current board is LCP2216`

### Artifact group

* `other`

### Notes / conflicts / follow-up

* в release использовать только `LCP2216`
---

### Statement ID

* `FM-018`

### Status

* `FACT`

### Statement

* линия питания платы формируется через step-down узел `24V-5V` на `U2 = TPS54331DR`

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 5, title PSU 24V-5V -> U2`; `U2 -> TPS54331DR`

### Artifact group

* `power`

### Notes / conflicts / follow-up

* sheet title даёт входной токен `24V-5V`, BOM подтверждает тип микросхемы

---

### Statement ID

* `FM-019`

### Status

* `FACT`

### Statement

* от линии `+5V` сформирована локальная линия `+3V3` через `U3 = LM1117MPX-3.3`

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 5 -> label +3V3 and U3`; `U3 -> LM1117MPX-3.3`

### Artifact group

* `power`

### Notes / conflicts / follow-up

* итоговая топология `24V -> +5V -> +3V3` подтверждена как `as-built`

---

### Statement ID

* `FM-020`

### Status

* `FACT`

### Statement

* линия `+5V` имеет TVS-защиту `TVS1`

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 5 -> TVS1 on +5V`; `TVS1 -> SMAJ5.0CA`

### Artifact group

* `protection`

### Notes / conflicts / follow-up

* защита выходной линии видна прямо на power sheet

---

### Statement ID

* `FM-021`

### Status

* `FACT`

### Statement

* основной вычислительный узел платы реализован на `U1 = ATSAM3X8EA-AU`

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 6 -> ARM M3 32bit 84 MHz / ATSAM3X8EA-AU`; `U1 -> ATSAM3X8EA-AU`

### Artifact group

* `processor`

### Notes / conflicts / follow-up

* MCU identity закрыта несколькими источниками

---

### Statement ID

* `FM-022`

### Status

* `FACT`

### Statement

* у процессорного модуля есть отдельные тактовые источники `12.0 MHz` и `32.768 kHz`

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 6 -> XTL6 12.0 MHz, XT1 32.768Khz`; `XTL6`, `XT1`

### Artifact group

* `processor`

### Notes / conflicts / follow-up

* частоты читаются прямо на schematic sheet и поддержаны BOM

---

### Statement ID

* `FM-023`

### Status

* `FACT`

### Statement

* на процессорном листе присутствует JTAG и сигналы `CS_ETH`, `CS_ETH_2`, `CS_SD`, `SD_Detect`

### Source anchor

* File: `sources/schematic/Schematic.pdf`
* Anchor: `page 6 -> JTAG, CS_ETH, CS_ETH_2, CS_SD, SD_Detect`

### Artifact group

* `processor`

### Notes / conflicts / follow-up

* это подтверждает аппаратную привязку MCU к периферийным узлам

---

### Statement ID

* `FM-024`

### Status

* `FACT`

### Statement

* на плате реализованы два Ethernet-канала на отдельных контроллерах `W5500` и двух разъёмах `RJ45 MagJack`

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 1 -> U16 + ETH2`; `page 2 -> U11 + ETH1`; `U11,U16 -> W5500`; `ETH1,ETH2 -> RJ45 MagJack`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* физическое наличие двух Ethernet-портов закрыто

---

### Statement ID

* `FM-025`

### Status

* `FACT`

### Statement

* каждый Ethernet-канал имеет собственный `25.0 MHz` тактовый генератор

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 1 -> XTL5 Clock Oscillators 25.0 MHZ`; `page 2 -> XTL4 Clock Oscillators 25.0 MHZ`; `XTL4,XTL5 -> 25.0 MHz`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* два Ethernet-порта аппаратно одинаковы; operator review разрешает описывать их назначение как гибкое
---

### Statement ID

* `FM-026`

### Status

* `FACT`

### Statement

* schematic PDF показывает шесть линий `RS1...RS6` на физическом уровне `RS-485`

### Source anchor

* File: `sources/schematic/Schematic.pdf`
* Anchor: `page 4 -> RS485 Lines`; `page 7 -> RS1_P/N ... RS6_P/N`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* operator mapping для release: `RS1...RS4` — внешние устройства, `RS5` — `HMI`, `RS6` — `PC`
---

### Statement ID

* `FM-027`

### Status

* `FACT`

### Statement

* BOM содержит `7` трансиверов `SN65HVD3082EDR`; седьмой канал используется линией `X2X`

### Source anchor

* File: `sources/bom/BOM.csv`; `sources/schematic/Schematic.pdf`
* Anchor: `U4, U5, U6, U7, U8, U9, U10 -> SN65HVD3082EDR`; `page 4 -> CAN Bus X2X`; `page 7 -> X_L, X_H`; `operator clarification -> seven RS-485-physical lines total`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* release-формулировка по текущему guide: `RS1...RS4`, `HMI`, `PC`, `X2X`
---

### Statement ID

* `FM-028`

### Status

* `FACT`

### Statement

* в RS-485 подсистеме присутствует `U18 = SC16IS752IPW,128` и кварц `14.7456 MHz`

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 7 -> U18 and XTL2 14.7456MHz`; `U18 -> SC16IS752IPW,128`; `XTL2 -> 14.7456MHz`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* это подтверждает отдельный мост/расширитель последовательных линий

---

### Statement ID

* `FM-029`

### Status

* `FACT`

### Statement

* на плате есть отдельная дифференциальная линия `X2X` с сигналами `X_L / X_H`

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/extras/Manual_Lorentz.pdf`
* Anchor: `page 4 -> CAN Bus X2X`; `page 7 -> X_L, X_H`; `manual page 7 -> X2X сеть модулей`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* назначение линии как межмодульной поддержано extras, но детальная системная семантика вне scope Stage two

---

### Statement ID

* `FM-030`

### Status

* `FACT`

### Statement

* сервисный USB реализован через разъём `USB B` и прямые линии `D+ / D-` к процессорному модулю

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 4 -> USB - Firmware/Debug and J1`; `page 6 -> D+_P, D-_N`; `J1 -> USB B Socket`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* use-case `firmware/debug` читается с листа подключений

---

### Statement ID

* `FM-031`

### Status

* `FACT`

### Statement

* USB data lines имеют отдельные элементы line-protection `Z1` и `Z2`

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 6 -> Z1, Z2 on D+/D-`; `Z1,Z2 -> Chip Guard 0603 5 Volts`

### Artifact group

* `protection`

### Notes / conflicts / follow-up

* узел защиты USB подтверждён и схемой, и BOM

---

### Statement ID

* `FM-032`

### Status

* `FACT`

### Statement

* слот microSD подключён по SPI и имеет отдельный сигнал детекта карты

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 8 -> MISO, MOSI, SPCK, CS_SD, SD_Detect`; `SD1 -> uSD card holder`

### Artifact group

* `storage`

### Notes / conflicts / follow-up

* аппаратная привязка storage узла закрыта

---

### Statement ID

* `FM-033`

### Status

* `FACT`

### Statement

* на плате реализована локальная световая индикация из `10` светодиодов и `5` light guide

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 3 -> LED1...LED10, LG1...LG5`; `BOM -> LED1...LED10, LG1...LG5`

### Artifact group

* `indication`

### Notes / conflicts / follow-up

* подсистема индикации подтверждена на schematic и BOM

---

### Statement ID

* `FM-034`

### Status

* `FACT`

### Statement

* коммутация индикаторов выполнена через `7` MOSFET `BSS138`

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 3 -> T1...T7`; `T1...T7 -> BSS138`

### Artifact group

* `indication`

### Notes / conflicts / follow-up

* это подтверждает дискретную аппаратную схему индикации

---

### Statement ID

* `FM-035`

### Status

* `FACT`

### Statement

* BOM содержит `22` TVS-диода и `15` PPTC-предохранителей, а schematic PDF показывает их использование на линиях RS-485 и на линии `+5V`

### Source anchor

* File: `sources/bom/BOM.csv`; `sources/schematic/Schematic.pdf`
* Anchor: `TVS1...TVS22 -> quantity 22`; `F1...F15 -> quantities 14 + 1`; `page 5 -> TVS1`; `page 7 -> TVS2...TVS22 and F1...F15`

### Artifact group

* `protection`

### Notes / conflicts / follow-up

* наличие распределённой защиты линий закрыто

---

### Statement ID

* `FM-036`

### Status

* `FACT`

### Statement

* на линиях `RS-485` подтверждена защита TVS и PPTC; утверждение о гальванической изоляции интерфейсов в release не использовать

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`
* Anchor: `page 7 -> TVS2...TVS22 and F1...F15`; `BOM -> TVS1...TVS22, F1...F15`; `operator clarification -> keep protection wording, drop isolation claim`

### Artifact group

* `protection`

### Notes / conflicts / follow-up

* operator review закрыл вопрос на уровне release-scope через отказ от неподтверждённого isolation-claim
---

### Statement ID

* `FM-037`

### Status

* `FACT`

### Statement

* stackup-файл и gerber report вместе подтверждают шестислойную проводящую структуру платы

### Source anchor

* File: `sources/stackup/LCP.stackup`; `sources/gerber/Gerber.zip`
* Anchor: `Top, Gnd, Power, HighSpeed, High speed, Bottom`; `.REP -> Generating: Top, Gnd, Power, HighSpeed, High speed, Bottom`

### Artifact group

* `stackup`

### Notes / conflicts / follow-up

* это достаточное основание для release-формулировки `шестислойная плата`

---

### Statement ID

* `FM-038`

### Status

* `FACT`

### Statement

* для каждого проводящего слоя в stackup задана толщина меди `0.018 mm`

### Source anchor

* File: `sources/stackup/LCP.stackup`
* Anchor: `LAYER_V8_3COPTHICK=0.018mm`, `LAYER_V8_5COPTHICK=0.018mm`, `LAYER_V8_7COPTHICK=0.018mm`, `LAYER_V8_9COPTHICK=0.018mm`, `LAYER_V8_11COPTHICK=0.018mm`, `LAYER_V8_13COPTHICK=0.018mm`

### Artifact group

* `stackup`

### Notes / conflicts / follow-up

* copper thickness закрыта freeze-source без внешних допущений

---

### Statement ID

* `FM-039`

### Status

* `FACT`

### Statement

* stackup задаёт четыре внутренних диэлектрических промежутка `0.127 mm`, `0.127 mm`, `0.864 mm`, `0.127 mm`, а также верхний и нижний solder resist по `0.01016 mm`

### Source anchor

* File: `sources/stackup/LCP.stackup`
* Anchor: `LAYER_V8_2DIELHEIGHT=0.01016mm`, `LAYER_V8_4DIELHEIGHT=0.127mm`, `LAYER_V8_6DIELHEIGHT=0.127mm`, `LAYER_V8_8DIELHEIGHT=0.864mm`, `LAYER_V8_10DIELHEIGHT=0.127mm`, `LAYER_V8_14DIELHEIGHT=0.01016mm`

### Artifact group

* `stackup`

### Notes / conflicts / follow-up

* material names не везде заполнены, но геометрия задана

---

### Statement ID

* `FM-040`

### Status

* `FACT`

### Statement

* drill report подтверждает `374` plated holes с инструментами от `0.2 mm` до `3.3 mm`

### Source anchor

* File: `sources/gerber/Gerber.zip`
* Anchor: `.DRR -> Totals 374; T1 0.2mm ... T14 3.3mm`

### Artifact group

* `mechanical`

### Notes / conflicts / follow-up

* manufacturing-level drill факт закрыт

---

### Statement ID

* `FM-041`

### Status

* `FACT`

### Statement

* текущая плата и guide относятся к ревизии `Rev.7.1`; токен `rev 7.2` внутри fabrication path не считать ревизией платы

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/gerber/Gerber.zip`
* Anchor: `page 9 -> LCP2216 Rev.7.1`; `.RUL -> path token LCP rev 7.2`; `operator clarification -> canonical revision is Rev.7.1`

### Artifact group

* `mechanical`

### Notes / conflicts / follow-up

* для release и Stage three использовать только `Rev.7.1`
---

### Statement ID

* `FM-042`

### Status

* `FACT`

### Statement

* для current guide принять release-формулировку по интерфейсам: `два Ethernet-порта, сервисный USB, линии RS1...RS4 для внешних устройств, отдельные линии HMI и PC на физическом уровне RS-485 и линия X2X для подключения модулей`

### Source anchor

* File: `sources/schematic/Schematic.pdf`; `sources/bom/BOM.csv`; `sources/extras/Manual_Lorentz.pdf`
* Anchor: `pages 1,2,4,7,8`; `U4...U10`; `manual page 7 -> X2X`; `operator clarification -> release wording accepted`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* это закрывает release wording without claim of seven identical external ports