# RELEASE MANIFEST

## Document role

ℹ️ Этот файл фиксирует freeze-комплект Source of Truth для одной платы и одной ревизии.

---

## Board identity

### Board ID

* `LCP_rev7.1`

### Revision

* `rev7.1`

### Freeze date

* `2026-03-08`

### Freeze owner

* `none`

---

## Source bundle

### Primary incoming bundle

* `PCB_guide_Stage_1.2.zip`

### Source bundle type

* `zip`

### Entry point used for freeze

* `/README.md`
* `docs/authoring/pipeline/PIPELINE.md`

### Active stage

* `Stage 1 — Freeze Sources`

---

## Included groups

### `sources/cad/`

* Status: `present`
* Main files:
  * `sources/cad/Native.zip`
* Notes:
  * архив содержит native Altium-проект: `*.PrjPcb`, `*.PcbDoc`, `*.PcbLib`, `*.BomDoc`, `*.OutJob`, `*.SchDoc`
  * внутри архива виден проектный токен `МЦТР.00010572.00.001`
  * канонический native-комплект принят без внешнего переименования

### `sources/schematic/`

* Status: `partial`
* Main files:
  * `sources/schematic/Schematic.pdf`
* Notes:
  * присутствует читабельный schematic PDF
  * у файла `9` страниц, при этом внутри `Native.zip` присутствуют `12` файлов `*.SchDoc`
  * PDF-экспорт пригоден для чтения, но не считается полным доказательством покрытия всех листов без отдельной сверки на Stage `2`

### `sources/pcb/`

* Status: `present`
* Main files:
  * `sources/pcb/Job1.PDF`
* Notes:
  * присутствует PCB/layout export в PDF-формате
  * файл имеет `7` страниц
  * имя файла обобщённое, но accepted как часть текущего freeze-комплекта

### `sources/bom/`

* Status: `present`
* Main files:
  * `sources/bom/BOM.csv`
* Notes:
  * присутствует канонический BOM-export
  * файл использует `;` как разделитель колонок
  * заголовок читается как `Comment;Designator;Footprint;Quantity;Part Description;Part Number`

### `sources/gerber/`

* Status: `present`
* Main files:
  * `sources/gerber/Gerber.zip`
* Notes:
  * архив содержит fabrication-output комплект с drill/report файлами
  * внутри присутствуют `GTL`, `GBL`, `GTS`, `GTO`, `GBS`, `G1`, `G2`, `GP1`, `GP2`, `DRR`, `-Plated.TXT`, `Status Report.Txt`
  * внутри архива также виден проектный токен `МЦТР.00010572.00.001`

### `sources/stackup/`

* Status: `present`
* Main files:
  * `sources/stackup/LCP.stackup`
* Notes:
  * присутствует отдельный Altium stackup-файл
  * внутри читаются токены `STACKUPVERSION=1`, а также имена слоёв `Top`, `Gnd`, `Power`, `HighSpeed`, `Bottom`

### `sources/extras/`

* Status: `present`
* Main files:
  * `sources/extras/Manual_LCP.pdf`
  * `sources/extras/Manual_Lorentz.pdf`
* Notes:
  * extras приняты как reference/proof-материалы верхнего уровня
  * `Manual_LCP.pdf` на первой странице идентифицирует модульные свободно программируемые контроллеры `Lorentz`
  * `Manual_Lorentz.pdf` на первой странице идентифицирует процессорный модуль `LCP2116`

---

## Excluded groups and files

### Excluded from freeze

* `none`

### Conflicting candidates not included

* `none`

### Storage rule for excluded candidates

* excluded candidates are not kept inside `sources/*`
* original external handoff may still contain them outside the canonical repo freeze

---

## Known gaps

### Missing source groups

* `none`

### Known quality limits of the bundle

* `sources/schematic/Schematic.pdf` не подтверждает полное покрытие всех native schematic sheets без отдельной сверки
* токен ревизии `rev7.1` не найден в именах файлов внутри `sources/*`; идентичность ревизии сейчас держится на operator decision и manifest
* native CAD хранится как один ZIP-архив, что удобно для freeze, но снижает удобство пофайлового просмотра внутри репозитория

---

## Naming and normalization

### External renames accepted in freeze

* `none`

### Kept as original on purpose

* `Native.zip` — сохранён как исходный handoff-архив native CAD без косметического переименования
* `Job1.PDF` — сохранён как текущий canonical PCB export; переименование допустимо только вместе с обновлением manifest
* `Gerber.zip` — сохранён как текущий canonical fabrication archive внутри репозитория

---

## Freeze decision

### Decision

* `accepted with gaps`

### Why this decision

* минимально ожидаемые группы Stage `1` присутствуют: native CAD, schematic plot, PCB export, BOM, Gerber, stackup, extras
* внутри accepted-комплекта не найдено явных конфликтующих альтернатив или второй правды по тем же canonical путям
* главный quality limit относится к schematic PDF: он неочевидно покрывает весь набор native schematic sheets

### What changes require manifest update

* change in `sources/*`
* replacement of a canonical file
* addition of a previously missing source group
* revision-mixing cleanup that changes the accepted SoT set

---

## Notes

### Freeze notes

* внутри `sources/cad/Native.zip` и `sources/gerber/Gerber.zip` повторяется проектный токен `МЦТР.00010572.00.001`, что поддерживает внутреннюю согласованность freeze-комплекта
* на Stage `1` комплект признан рабочим Source of Truth; более глубокая инженерная декомпозиция переносится на Stage `2`
