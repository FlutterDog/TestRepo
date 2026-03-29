# STAGE PROMPT — Stage 5 (QC + Release Build)

Ты работаешь по канону репозитория:

* `docs/authoring/pipeline/PIPELINE.md`
* `/README.md`

Сейчас активная стадия: **Stage 5 — QC + Release Build**.

## GOAL

Stage five работает как итеративная clean-up и release-build стадия.

Она может запускаться повторно, пока не будет получен clean release.

Твоя задача:

* выполнить QC текущего Draft;
* убрать editorial residue;
* при необходимости усилить слабые главы, используя уже имеющийся factual base, `author_notes.md` и official references;
* сохранить clean figure links;
* сформировать релиз-пакет:
  * `docs/release/board_guide.md`
  * `docs/release/IMG/`
* заполнить `qc_report.md`.

Если release ещё не проходит quality bar, допускается сначала выпустить:

* `draft_v1.md`
* `qc_report.md`

и вернуть статус, что нужен ещё один Stage five pass.

---

## HARD RULES

### Один. Release должен быть чистым

В `docs/release/board_guide.md` запрещены:

* process-language;
* stage language;
* author instructions;
* status notes;
* open items;
* FACT / ASSUMPTION / UNKNOWN markers;
* видимые placeholder notes;
* remarks о том, чего не хватает для следующего прохода.

### Два. Любые жёсткие факты и числа допускаются только при подтверждении

### Три. Термины и единицы обязаны соответствовать `glossary.md`

### Четыре. Проверяй не только factual cleanliness, но и explanatory adequacy

Релиз не проходит QC, если:

* глава сводится к сухой инвентаризации;
* нет ключевых характеристик там, где они очевидно нужны;
* нет объяснения, что узел даёт пользователю или интегратору;
* нет ограничений и инженерных последствий там, где они критичны.

### Пять. Stage five может усиливать текст, но не имеет права выдумывать

Разрешено:

* переработать слабые главы;
* усилить поясняющие абзацы;
* использовать `author_notes.md`;
* использовать official component / module manuals;
* уточнить формулировки по существующему factual base;
* очистить и нормализовать figure references.

Если для усиления текста используется большой official manual / reference manual,
то для общего описания компонента и summary-характеристик
нужно опираться только на первые три содержательные страницы документа.

Глубокие разделы manual разрешено использовать только для точечного подтверждения
конкретной already-relevant board feature, ограничения или режима.

Запрещено:

* придумывать новые board facts;
* подменять board-specific текст общим пересказом datasheet;
* вносить claims, не подтверждённые текущим factual base;
* трогать `sources/*`.

### Шесть. Figure links нужно сохранять

Если figure asset ещё не подложен, но место и имя файла уже определены корректно, в release сохраняется clean markdown image-link без видимой служебной note-строки.

Stage five должен удалить служебную placeholder prose, но не должен без причины вырезать полезные figure links.

---

## RELEASE WRITING BAR

Релиз должен читаться как инженерное описание платы, а не как отчёт о процессе подготовки текста.

Целевой стиль:

* средний режим между board guide и system reference manual;
* без маркетинга;
* без сухого machine-like перечисления;
* с полезными характеристиками, таблицами, схемами и пояснениями там, где это реально помогает пользователю.

Если Stage four дал честный, но бедный текст, Stage five обязан попытаться усилить его на базе уже загруженных sources, official manuals и `author_notes.md`, а не только формально выписать замечание.

---

## OUTPUT FORMAT

Всегда выдай:

* `## docs/authoring/artifacts/stage_5_qc_release_build/qc_report.md`

Если release уже чистый, выдай дополнительно:

* `## docs/release/board_guide.md`

Если release ещё не чистый, но после прохода появился улучшенный clean-up draft, выдай вместо релиза:

* `## docs/authoring/artifacts/stage_5_qc_release_build/draft_v1.md`

После файлов обязательно дай короткий итог:

* `Stage status`
* `Assistant output`
* `Residual issues`
* `Next re-entry point`
* `Go / No-Go`

---

## QUALITY BAR

* Release читается как guide и самодостаточен.
* В релизе нет неподтверждённых жёстких фактов.
* В релизе нет process-language.
* В релизе нет бедных глав, сведённых к нескольким сухим фразам.
* Figure links и captions сохранены в чистом виде.
* `qc_report.md` отражает не только ошибки, но и содержательные слабые места текста.
* Если релиз ещё не готов, Stage five это честно фиксирует и отдаёт clean-up draft для следующего прохода.