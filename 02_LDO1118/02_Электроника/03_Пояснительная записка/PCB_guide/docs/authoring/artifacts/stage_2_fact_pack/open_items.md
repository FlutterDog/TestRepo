# OPEN ITEMS

## Назначение документа

ℹ️ Этот файл фиксирует все конфликтные, неполные или спорные места Stage two.

❗ Всё, что не закрыто источником, должно быть либо явно ограничено в future draft, либо закрыто дополнительным source input.

---

## Open items

### Item ID

* `OI-001`

### Topic

* конфликт board identity: `LCP2116` / `LCP2216` / `Rev.7.1` / `LCP rev 7.2`

### Why unknown

* manuals уверенно указывают `LCP2116`;
* внутри native CAD встречаются одновременно `LCP2116`, `LCP2216`, `Rev.7.1` и `LCP rev 7.2`.

### What source is needed

* author clarification, какой board token и какая revision должны считаться canonical для финальной пояснительной записки;
* при наличии — фото шелкографии платы или актуальный assembly drawing / title block.

### Impact on next stages

* `high`

### Status / owner / resolution note

* Status: `open`
* Owner: `author`
* Resolution note: до закрытия использовать осторожную формулировку `процессорный модуль LCP семейства Lorentz из freeze-комплекта`.

---

### Item ID

* `OI-002`

### Topic

* точное соотношение между внешними портами `RS-485` и внутренней шиной `X2X`

### Why unknown

* BOM даёт семь `RS-485` transceiver;
* manual одновременно говорит о портах `1...7` и о составе интерфейсов `6x RS485, 2x Ethernet, 1x USB, 1x X2X`.

### What source is needed

* official module specification или author clarification по назначению каждого физического канала;
* желательно лист схемы с connector naming и line assignment.

### Impact on next stages

* `high`

### Status / owner / resolution note

* Status: `open`
* Owner: `author`
* Resolution note: в draft не обещать автоматически семь внешних пользовательских `RS-485` портов; безопаснее писать про группу `RS-485` интерфейсов и отдельную `X2X` шину.

---

### Item ID

* `OI-003`

### Topic

* точная роль `SC16IS752`

### Why unknown

* компонент уверенно идентифицирован в BOM, но его точная функция внутри конкретной платы не закрыта текущим quick-pass по net-level connectivity.

### What source is needed

* component datasheet;
* schematic connectivity review или author note.

### Impact on next stages

* `medium`

### Status / owner / resolution note

* Status: `open`
* Owner: `assistant + author`
* Resolution note: пока описывать только как дополнительный serial interface bridge / UART interface IC без точного overclaim.

---

### Item ID

* `OI-004`

### Topic

* board-level подтверждение `FRAM 512 kB`

### Why unknown

* это свойство заявлено в product manual;
* при быстром просмотре BOM нет сразу очевидного отдельного FRAM part number.

### What source is needed

* full BOM review по памяти;
* component datasheet / schematic sheet, где показан memory device;
* author clarification, если память находится в составе другой сборки или ревизии.

### Impact on next stages

* `medium`

### Status / owner / resolution note

* Status: `open`
* Owner: `assistant + author`
* Resolution note: в draft использовать только manual-level формулировку о product memory architecture и не описывать FRAM как уже локально подтверждённый отдельный board component.

---

### Item ID

* `OI-005`

### Topic

* точная реализация RTC

### Why unknown

* manual говорит о резервировании времени от `CR2032`, но quick-pass BOM не дал явного RTC part number.

### What source is needed

* schematic connectivity review around battery-backed domain;
* author clarification.

### Impact on next stages

* `low`

### Status / owner / resolution note

* Status: `open`
* Owner: `assistant + author`
* Resolution note: safe claim — резервирование времени батареей присутствует на уровне модуля; unsafe claim — указывать конкретную RTC-микросхему без источника.

---

### Item ID

* `OI-006`

### Topic

* отсутствие official component references в `sources/extras/references/`

### Why unknown

* Stage two выполнен без datasheet-пакета на ключевые ИС.

### What source is needed

* datasheet / reference manuals на `ATSAM3X8E`, `W5500`, `SN65HVD3082E`, `SC16IS752`, `TPS54331`, `LM1117-3.3`.

### Impact on next stages

* `medium`

### Status / owner / resolution note

* Status: `open`
* Owner: `author`
* Resolution note: Stage three запускать можно; перед сильным Stage four желательно догрузить official references.
