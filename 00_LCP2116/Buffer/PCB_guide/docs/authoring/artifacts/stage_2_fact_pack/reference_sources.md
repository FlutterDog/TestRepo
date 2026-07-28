# REFERENCE SOURCES

## Назначение документа

ℹ️ Этот файл перечисляет разрешённые reference-источники для Stage two – Stage five.

❗ Freeze-комплект платы остаётся первичным Source of Truth для board implementation facts.

---

## Currently loaded official sources

### Reference ID

* `RS-001`

### Source type

* `module manual`

### Object

* `Lorentz modular freely programmable controllers family`

### Canonical location

* `sources/extras/Manual_LCP.pdf`

### What may be taken from this source

* семейная принадлежность изделия;
* product-line framing;
* общая конструктивная и системная роль модулей `Lorentz`.

### What may NOT be claimed from this source automatically

* конкретная реализация данной платы;
* точные component-level характеристики без подтверждения BOM / schematic / official datasheet.

---

### Reference ID

* `RS-002`

### Source type

* `module manual`

### Object

* `LCP2116 processor module`

### Canonical location

* `sources/extras/Manual_Lorentz.pdf`

### What may be taken from this source

* product-level role модуля;
* заявленные интерфейсы;
* product-level технические характеристики;
* allowed wording around application role, uSD logging, RTC backup, USB service access.

### What may NOT be claimed from this source automatically

* что каждая manual-level характеристика уже подтверждена как локальный board implementation fact на уровне конкретного BOM / schematic revision;
* что нет конфликта между manual naming и native CAD naming.

---

## Missing but recommended official sources

### Reference ID

* `RS-MISS-001`

### Recommended source

* official datasheet / reference manual for `ATSAM3X8E`

### Why needed

* чтобы безопасно опираться на MCU architecture, memory map, peripheral set и clock-level claims.

### Suggested location

* `sources/extras/references/`

---

### Reference ID

* `RS-MISS-002`

### Recommended source

* official datasheet for `W5500`

### Why needed

* чтобы отделять возможности контроллера от возможностей, реально выведенных на плату.

### Suggested location

* `sources/extras/references/`

---

### Reference ID

* `RS-MISS-003`

### Recommended source

* official datasheet for `SN65HVD3082E`

### Why needed

* чтобы уверенно писать electrical behavior и supported bus conditions линии `RS-485`.

### Suggested location

* `sources/extras/references/`

---

### Reference ID

* `RS-MISS-004`

### Recommended source

* official datasheet for `SC16IS752`

### Why needed

* чтобы корректно интерпретировать роль дополнительного serial bridge.

### Suggested location

* `sources/extras/references/`

---

### Reference ID

* `RS-MISS-005`

### Recommended source

* official datasheet for `TPS54331` and `LM1117-3.3`

### Why needed

* чтобы делать более сильные power-architecture claims без инженерной фантазии.

### Suggested location

* `sources/extras/references/`

---

## Style references status

ℹ️ Для document composition уже доступен style exemplar:

- `docs/authoring/pipeline/style_references/Ti Style.pdf`

ℹ️ Он может использоваться для:

- плотности текста;
- структуры section-to-figure flow;
- выбора типов таблиц, схемных фрагментов и explanatory visuals.

❗ Он не является factual proof-source.
