# Stage 4 — Draft (`draft_v0.md`)

## Purpose

Stage four создаёт первый полный reader-facing draft.

Это ещё authoring-стадия, но основной файл уже должен читаться как нормальная пояснительная записка, а не как отчёт о подготовке документа.

Stage four выпускает два разных артефакта:

* `draft_v0.md` — основной читабельный текст;
* `stage_4_handoff.md` — служебный список editorial issues, missing assets и замечаний перед Stage five.

---

## Inputs

### Обязательные

* `outline.md`
* `fact_pack.md`
* `glossary.md`
* `open_items.md`
* `reference_sources.md`
* `design_intent.md`
* `file_map.md`
* `author_notes.md`

### Обязательные source layers для сильного текста

* board implementation sources из freeze-комплекта;
* official component and module references из `sources/extras/references/` и / или `reference_sources.md`.

Для Stage four official references считаются не только proof-base, но и обязательным материалом для живого explanatory prose по компонентам, power tree, processor, interfaces, clocks, resets, protection, memory и другим ключевым узлам.

### Опциональные, но полезные

* изображения из `docs/authoring/artifacts/IMG/`
* figure briefs / table briefs, если они были подготовлены на Stage three

---

## Procedure

### Шаг ноль. Проверить полноту factual base

Перед Draft ассистент обязан проверить:

* есть ли `author_notes.md` по текущему outline;
* покрыты ли ключевые компоненты и модули official references;
* хватает ли source base для глав, где ожидаются характеристики, режимы, архитектурные пояснения и ограничения.

Если по ключевому узлу reference-слой слишком слабый, ассистент должен явно зафиксировать это в handoff, а не обходить тему бедным текстом.

### Шаг один. Писать строго по outline

### Шаг два. Использовать все разрешённые слои данных

На Stage four разрешено и нужно использовать:

* implemented facts;
* component capability facts;
* board-supported capabilities;
* design intent;
* author notes;
* official component / module manuals как источник нейтральных технических описаний.

### Шаг три. Для каждого крупного узла писать не менее пяти смысловых слоёв

* что это за узел;
* как реализован;
* какие характеристики важны;
* что это даёт пользователю, интегратору или новому владельцу;
* какие ограничения и инженерные последствия у модификаций.

### Шаг четыре. Использовать official manuals не как свалку features, а как материал для сильной прозы

Разрешено брать из manual / datasheet:

* краткое нейтральное описание класса микросхемы или подсистемы;
* важные архитектурные акценты;
* подтверждённые характеристики;
* полезные формулировки для объяснения роли узла.

Запрещено:

* копировать marketing language;
* вставлять целые feature-list blocks;
* превращать все возможности компонента в обещания по плате.

Правильный результат — не пересказ datasheet, а board-specific текст, усиленный official references.

### Шаг пять. Использовать author notes как усилитель текста

`author_notes.md` обязаны реально влиять на формулировки, акценты, design rationale, ограничения и practical nuances.

### Шаг шесть. Корректно обходиться с figures и tables

* Если asset уже готов, использовать обычную рабочую ссылку в тексте.
* Если asset запланирован, но ещё не готов, Draft должен сохранять для него логическое место и корректную markdown-ссылку.
* В основной текст разрешён только clean asset reference block:
  * HTML comment с Figure ID, type, owner и status;
  * markdown image-link или table reference с целевым путём в `IMG/`;
  * без видимой note-строки про подготовку файла.

Все remarks о недостающих assets должны идти в `stage_4_handoff.md`.

### Шаг семь. Жёстко разделять main draft и handoff

В `draft_v0.md` недопустимы:

* process-language;
* status notes;
* author instructions;
* замечания о текущем проходе;
* разделы типа «итоговый статус Draft»;
* видимые placeholder notes.

В `stage_4_handoff.md` обязаны жить:

* missing assets;
* weak chapters;
* места, где нужен дополнительный editor / author pass;
* замечания перед Stage five;
* статус Go / No-Go.

---

## Outputs

### Основные артефакты

* `docs/authoring/artifacts/stage_4_draft/draft_v0.md`
* `docs/authoring/artifacts/stage_4_draft/stage_4_handoff.md`

### Сопутствующие обновления при необходимости

* `file_map.md`
* `open_items.md`
* `glossary.md`
* `fact_pack.md`

---

## Что ассистент должен сказать в конце Stage four

Ассистент обязан явно сообщить:

* получился ли Draft reader-facing и пригодным для перехода дальше;
* какие assets всё ещё не хватает;
* какие главы ещё слабоваты;
* можно ли уже идти на Stage five или лучше сделать ещё один author / editor pass.

---

## DoD

Stage four завершён качественно, если:

* `draft_v0.md` покрывает весь outline;
* текст уже читается как инженерный документ;
* в разделах есть не только реализация, но и характеристики, возможности и ограничения;
* `author_notes.md` реально использованы;
* official manuals / datasheets реально использованы для усиления описательных частей;
* в основном Draft уже стоят figure links и captions;
* в основном Draft нет process-language и видимых служебных пометок;
* все editorial remarks вынесены в `stage_4_handoff.md`.

---

## Как использовать style exemplars на Stage four

Если в проекте есть документы в `docs/authoring/pipeline/style_references/`, ассистент может использовать их только для:

* выбора плотности текста;
* выбора места для figures, tables и diagrams;
* выбора типа explanatory inserts.

Ассистент не должен переносить из них factual claims по вашей плате или по применённым компонентам.
