# STAGE PROMPT — Stage 4 (Draft)

Ты работаешь по канону репозитория:

* `docs/authoring/pipeline/PIPELINE.md`
* `/README.md`

Сейчас активная стадия: **Stage 4 — Draft**.

## GOAL

Сформировать Stage four как reader-facing draft, а не как process-report.

Обязательные выходы:

* `docs/authoring/artifacts/stage_4_draft/draft_v0.md`
* `docs/authoring/artifacts/stage_4_draft/stage_4_handoff.md`

`draft_v0.md` должен читаться как нормальная пояснительная записка.

`stage_4_handoff.md` должен содержать editorial residue, missing assets, weak chapters и замечания перед Stage five.

Draft должен быть написан строго по `outline.md` и опираться на:

* `fact_pack.md`
* `glossary.md`
* `open_items.md`
* `reference_sources.md`
* `design_intent.md`
* `file_map.md`
* `author_notes.md`
* official component / module references из `sources/extras/references/` и разрешённого factual base

---

## HARD RULES

### Один. Scope: только hardware

### Два. Main draft не может быть отчётом о процессе

В `draft_v0.md` запрещены:

* process-language;
* status notes;
* author instructions;
* фразы вида «в текущем проходе», «перед Stage five», «нужно подготовить файл»;
* разделы вида «Итоговый статус Draft»;
* видимые placeholder notes.

Все такие вещи должны идти в `stage_4_handoff.md`.

### Три. Любые жёсткие факты и числа — только при подтверждении

### Четыре. Различай уровни утверждений

Если пишешь:

* что стоит на плате — опирайся на board implementation facts;
* что умеет компонент — опирайся на official component sources;
* что реально может плата — формулируй только как board-supported capability;
* зачем узел нужен — опирайся на design intent / author notes.

### Пять. `author_notes.md` обязателен к использованию

Недопустимо считать `author_notes.md` факультативным слоем или игнорировать его, если он присутствует.

### Шесть. Official manuals и datasheets обязаны реально усиливать текст

Используй official references не только как источник проверки чисел, но и как материал для сильного explanatory prose.

Разрешено брать из них:

* краткое нейтральное описание класса компонента;
* архитектурные особенности, релевантные плате;
* подтверждённые характеристики;
* режимы, интерфейсы, power / clock / reset / protection features, если они реально помогают понять реализацию.

Запрещено:

* копировать marketing language;
* вставлять длинные сырьевые feature-list blocks;
* пересказывать весь manual вместо описания платы;
* превращать capability компонента в capability платы без board-level привязки.

Для больших official manuals / reference manuals запрещено строить
общее описание компонента на глубоком чтении документа целиком.

Для вступительных explanatory paragraphs и summary-характеристик
используй только первые три содержательные страницы manual
(introduction / description / overview / features summary).

Глубже можно идти только для точечной верификации конкретной релевантной функции
или ограничения, уже нужной для описания платы.

### Семь. Не делать длинные реестры RefDes

### Восемь. Иллюстрации вставляются рабочими ссылками или clean asset reference blocks

Если asset уже существует, вставляй обычную markdown-ссылку на файл в `IMG/`.

Если asset заложен в Figures Plan, но ещё не готов, вставляй clean asset reference block прямо в Draft:

* HTML comment с Figure ID, type, owner и status;
* markdown image-link или table reference с целевым путём в `IMG/`;
* без видимой служебной note-строки.

### Девять. Missing assets не теряются, но и не загрязняют main draft

Недопустимо тихо потерять важный рисунок, схему или таблицу.

Но equally недопустимо превращать main draft в список напоминаний.

Все missing-asset remarks должны попадать в `stage_4_handoff.md`.

---

## DRAFT WRITING BAR

### Главное правило

`draft_v0.md` должен читаться как связное инженерное объяснение платы, а не как:

* checklist;
* конспект по fact pack;
* process-report;
* handoff note;
* сухой список узлов.

### Как писать хороший раздел

Для каждого крупного узла / подсистемы стремиться к такой форме:

* короткое объяснение, что это за узел и зачем он нужен;
* один или два абзаца, как он реализован на плате;
* ключевые характеристики, важные для понимания платы;
* объяснение, что это даёт пользователю, интегратору или новому владельцу;
* ограничения, инварианты и последствия модификаций.

### Как использовать official references правильно

Хороший текст Stage four должен уметь превращать материалы из component manuals и datasheets в board-specific prose.

Пример правильного подхода:

* взять description и features overview из официального manual;
* отобрать только релевантные данной плате аспекты;
* переписать их в нейтральной форме;
* связать с конкретной реализацией платы, а не оставлять общим пересказом чипа.

### Какой стиль считать целевым

Целевой стиль — средний режим между board guide и system reference manual:

* без маркетинга;
* без копипаста даташитов;
* с характеристиками, таблицами и архитектурным смыслом там, где это реально полезно.

---

## OUTPUT FORMAT

Выдай обязательно:

* `## docs/authoring/artifacts/stage_4_draft/draft_v0.md`
* `## docs/authoring/artifacts/stage_4_draft/stage_4_handoff.md`

Если в ходе работы реально потребовались сопутствующие обновления, после основных артефактов допустимо выдать:

* `file_map.md`
* `open_items.md`
* `glossary.md`
* `fact_pack.md`

После файлов обязательно дай короткий handoff следующего шага:

* `Stage status`
* `Assistant output`
* `Author action now`
* `Where to put it`
* `Next re-entry point`
* `Go / No-Go`

---

## QUALITY BAR

* Draft полностью покрывает `outline.md`.
* В каждой крупной главе есть не только реализация, но и характеристики, возможности и ограничения.
* `author_notes.md` реально использованы.
* Official manuals / datasheets реально использованы для усиления описательных частей.
* Текст не сухой, но и не рекламный.
* Нет недоказанных чисел и категоричных утверждений без источника.
* В `draft_v0.md` нет process-language и видимых служебных placeholder notes.
* Все остаточные замечания вынесены в `stage_4_handoff.md`.

### Дополнительное правило. Применение style exemplars

Если в проекте есть `docs/authoring/pipeline/style_references/`, используй их только для композиции Draft: глубина объяснения, placement of figures / tables, balance between prose and visual assets.

Не используй их как factual source по вашей плате.