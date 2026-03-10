# OPEN ITEMS

## Document role

ℹ️ Этот файл хранит всё, что было вынесено на проверку в конце Stage two, и фиксирует итоговый статус после operator review.

❗ Одна запись = один вопрос, даже если он уже закрыт или осознанно снят из scope release.

---

## Итог после operator review

* Активных blocking open items для перехода на Stage three нет.
* Все спорные пункты либо закрыты оператором, либо сняты из release-scope.
* Для Stage three и Stage four запрещено возвращать в текст неподтверждённое утверждение о гальванической изоляции интерфейсов.
* Для линии питания в release использовать только подтверждённую формулировку про номинальную линию `24V` и локальные линии `+5V` и `+3V3`, без численного datasheet-range.

---

## Review log

### Item ID

* `OI-001`

### Topic

* неполное покрытие native schematic sheets в `Schematic.pdf`

### Why unknown

* На предыдущем проходе archive listing содержал лишние `SchDoc`, из-за чего возникло расхождение между native bundle и schematic PDF.

### What source is needed

* none

### Impact on next stages

* `low impact`

### Status / owner / resolution note

* Status: `resolved`
* Owner: `operator`
* Resolution note: оператор подтвердил, что лишние native schematic files были удалены как случайно загруженные; активный набор листов теперь соответствует schematic PDF.

---

### Item ID

* `OI-002`

### Topic

* конфликт имени изделия: `LCP2216` против `LCP2116`

### Why unknown

* PCB artwork показывает `LCP2216 Rev.7.1`, а один из reference-manual использует токен `LCP2116`.

### What source is needed

* none

### Impact on next stages

* `terminology risk`

### Status / owner / resolution note

* Status: `resolved`
* Owner: `operator`
* Resolution note: каноническое имя текущей платы — `LCP2216`; manual с токеном `LCP2116` использовать только как reference по аналогичному модулю другой формы исполнения.

---

### Item ID

* `OI-003`

### Topic

* конфликт ревизии: `Rev.7.1` против `rev 7.2`

### Why unknown

* PCB artwork и Board ID указывают `Rev.7.1`, а один токен в fabrication-export path давал `rev 7.2`.

### What source is needed

* none

### Impact on next stages

* `terminology risk`

### Status / owner / resolution note

* Status: `resolved`
* Owner: `operator`
* Resolution note: каноническая ревизия платы для current guide — `Rev.7.1`; токен `rev 7.2` трактовать как артефакт export-path, а не как действующую ревизию платы.

---

### Item ID

* `OI-004`

### Topic

* утверждение об изоляции интерфейсов

### Why unknown

* По operator review подтверждена защита линий `RS-485` через TVS и PPTC, но отдельное утверждение о гальванической изоляции не требуется для release и не должно вводиться без специального proof-source.

### What source is needed

* none for current release-scope

### Impact on next stages

* `release exclusion`

### Status / owner / resolution note

* Status: `dropped`
* Owner: `assistant`
* Resolution note: в Stage three и Stage four использовать только подтверждённую формулировку про защиту линий; утверждение о гальванической изоляции в release не включать.

---

### Item ID

* `OI-005`

### Topic

* точная release-формулировка по количеству RS-485 портов

### Why unknown

* Предыдущая safe-формулировка опиралась только на видимый schematic PDF и BOM без точного функционального mapping.

### What source is needed

* none

### Impact on next stages

* `terminology risk`

### Status / owner / resolution note

* Status: `resolved`
* Owner: `operator`
* Resolution note: принять формулировку: на физическом уровне `RS-485` реализовано семь линий — `RS1`…`RS4` для внешних устройств, отдельная линия `HMI`, отдельная линия `PC` и линия `X2X` для подключения модулей.

---

### Item ID

* `OI-006`

### Topic

* точный рабочий диапазон входной линии питания

### Why unknown

* Freeze-комплект подтверждает только номинальную линию `24V`, step-down `24V-5V` и локальные линии `+5V` и `+3V3`, но не содержит самостоятельного board-level specification с численным диапазоном.

### What source is needed

* none for current release-scope

### Impact on next stages

* `low impact`

### Status / owner / resolution note

* Status: `dropped`
* Owner: `assistant`
* Resolution note: в release не давать численный диапазон входа; использовать только подтверждённую архитектурную формулировку про линию `24V`, `+5V` и `+3V3`.

---

### Item ID

* `OI-007`

### Topic

* происхождение page `9` внутри `Schematic.pdf`

### Why unknown

* Ранее было непонятно, является ли page `9` частью схемы или отдельным добавленным видом.

### What source is needed

* none

### Impact on next stages

* `low impact`

### Status / owner / resolution note

* Status: `resolved`
* Owner: `operator`
* Resolution note: page `9` трактовать как assembly view / PCB artwork view; он допустим как proof-source для маркировки `LCP2216 Rev.7.1` и физического расположения элементов.

---

### Item ID

* `OI-008`

### Topic

* точное функциональное различие листов `Ethernet.SchDoc` и `ETH_HMI.SchDoc`

### Why unknown

* Предыдущий проход подтверждал два Ethernet-канала, но не фиксировал operator-level семантику их использования.

### What source is needed

* none

### Impact on next stages

* `low impact`

### Status / owner / resolution note

* Status: `resolved`
* Owner: `operator`
* Resolution note: аппаратно это два одинаковых Ethernet-порта; их назначение гибкое. Обычно они используются для Ethernet-панели оператора, АРМ диспетчера или клиентов `IEC 104`.
