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

* freeze-комплект содержит native Altium project, а не только read-only exports

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

* native CAD bundle включает отдельные schematic sheets `Processor`, `Power Supply`, `RS485`, `Ethernet`, `ETH_HMI`, `LEDs`, `Plugs`, `SD Holder`

### Source anchor

* File: `sources/cad/Native.zip`
* Anchor: list of `*.SchDoc` files inside archive

### Artifact group

* `other`

### Notes / conflicts / follow-up

* это подтверждает sheet-level decomposition проекта по ключевым подсистемам

---

### Statement ID

* `FM-003`

### Status

* `FACT`

### Statement

* процессорная часть платы реализована на `ATSAM3X8EA-AU`

### Source anchor

* File: `sources/bom/BOM.csv`
* Anchor: `U1 -> ATSAM3X8EA-AU -> ARM M3 32bit 84 MHz`

### Artifact group

* `processor`

### Notes / conflicts / follow-up

* manual называет модуль `ARM Cortex M3` на `84 MHz`

---

### Statement ID

* `FM-004`

### Status

* `FACT`

### Statement

* на плате установлены два Ethernet-контроллера `W5500`

### Source anchor

* File: `sources/bom/BOM.csv`
* Anchor: `U11, U16 -> W5500`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* component capability layer ограничен BOM-description, пока нет official datasheet

---

### Statement ID

* `FM-005`

### Status

* `FACT`

### Statement

* на плате установлены два разъёма `RJ45 MagJack`

### Source anchor

* File: `sources/bom/BOM.csv`
* Anchor: `ETH1, ETH2 -> J0011D21BNL -> RJ45 MagJack`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* согласуется с manual-level claim о двух Ethernet ports

---

### Statement ID

* `FM-006`

### Status

* `FACT`

### Statement

* hardware-level реализация последовательных интерфейсов включает семь `RS-485` transceiver `SN65HVD3082EDR`

### Source anchor

* File: `sources/bom/BOM.csv`
* Anchor: `U4, U5, U6, U7, U8, U9, U10 -> SN65HVD3082EDR`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* user-facing mapping этих каналов конфликтует с manual summary и вынесен в open items

---

### Statement ID

* `FM-007`

### Status

* `FACT`

### Statement

* на плате присутствует дополнительный serial interface IC `SC16IS752`

### Source anchor

* File: `sources/bom/BOM.csv`
* Anchor: `U18 -> SC16IS752IPW,128 -> UART Interface IC I2C/SPI`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* точная функциональная роль в составе платы пока не зафиксирована как FACT

---

### Statement ID

* `FM-008`

### Status

* `FACT`

### Statement

* силовая архитектура включает входной buck regulator `TPS54331DR` и линейный стабилизатор `LM1117MPX-3.3`

### Source anchor

* File: `sources/bom/BOM.csv`
* Anchor: `U2 -> TPS54331DR`; `U3 -> LM1117MPX-3.3`

### Artifact group

* `power`

### Notes / conflicts / follow-up

* manual дополнительно задаёт product-level питание от `24 V`

---

### Statement ID

* `FM-009`

### Status

* `FACT`

### Statement

* на плате присутствуют `uSD` card holder и battery holder `CR2032`

### Source anchor

* File: `sources/bom/BOM.csv`
* Anchor: `SD1 -> DM3AT-SF-PEJM5`; `B1 -> Battery Holder CR2032`

### Artifact group

* `memory / service`

### Notes / conflicts / follow-up

* согласуется с manual-level claims про logging, configuration storage и RTC backup

---

### Statement ID

* `FM-010`

### Status

* `FACT`

### Statement

* на плате реализованы сервисные узлы локального обслуживания: `USB Type B`, кнопки и LED-domain

### Source anchor

* File: `sources/bom/BOM.csv`
* Anchor: `J1 -> USB B Socket`; `Btn1, Btn2 -> Tact Switch`
* File: `sources/cad/Native.zip`
* Anchor: `Native/LEDs.SchDoc`

### Artifact group

* `service`

### Notes / conflicts / follow-up

* manual относит USB к диагностике и техническому обслуживанию

---

### Statement ID

* `FM-011`

### Status

* `FACT`

### Statement

* плата содержит развитую protection-chain на TVS и PPTC элементах

### Source anchor

* File: `sources/bom/BOM.csv`
* Anchor: `TVS1 ... TVS22 -> SMAJ5.0CA`; `F1 ... F15 -> PPTC`

### Artifact group

* `protection`

### Notes / conflicts / follow-up

* без dedicated protection documentation нельзя делать quantified EMC claims

---

### Statement ID

* `FM-012`

### Status

* `FACT`

### Statement

* stackup-файл показывает многослойную PCB-структуру с внутренними слоями `Gnd`, `Power`, `HighSpeed`, `High speed`

### Source anchor

* File: `sources/stackup/LCP.stackup`
* Anchor: layer names inside stack definition

### Artifact group

* `pcb / stackup`

### Notes / conflicts / follow-up

* safe release-level claim: многослойная плата с выделенными power / ground / high-speed слоями

---

### Statement ID

* `FM-013`

### Status

* `FACT`

### Statement

* product manual описывает модуль как центральный процессорный модуль `LCP2116` семейства `Lorentz`

### Source anchor

* File: `sources/extras/Manual_Lorentz.pdf`
* Anchor: титул и раздел `Общая информация`

### Artifact group

* `identity`

### Notes / conflicts / follow-up

* conflict с native CAD tokens закрывается отдельно

---

### Statement ID

* `FM-014`

### Status

* `ASSUMPTION`

### Statement

* один из физических последовательных каналов может быть логически выделен под `X2X`, поэтому BOM-count `RS-485` transceiver не должен автоматически читаться как количество внешних пользовательских портов

### Source anchor

* File: `sources/bom/BOM.csv`
* Anchor: seven `SN65HVD3082EDR`
* File: `sources/extras/Manual_Lorentz.pdf`
* Anchor: `6x RS485, 2x Ethernet, 1x USB, 1x X2X`

### Artifact group

* `interfaces`

### Notes / conflicts / follow-up

* требует author clarification или более детального sheet review

---

### Statement ID

* `FM-015`

### Status

* `ASSUMPTION`

### Statement

* freeze-комплект относится к одному и тому же изделию, но содержит унаследованные identity / revision tokens из разных редакций

### Source anchor

* File: `sources/cad/Native.zip`
* Anchor: strings `LCP2116`, `LCP2216`, `Rev.7.1`, `LCP rev 7.2` inside native files
* File: `sources/extras/Manual_Lorentz.pdf`
* Anchor: title `LCP2116`

### Artifact group

* `identity`

### Notes / conflicts / follow-up

* до закрытия конфликта в release лучше использовать осторожную формулировку без жёсткой revision claim
