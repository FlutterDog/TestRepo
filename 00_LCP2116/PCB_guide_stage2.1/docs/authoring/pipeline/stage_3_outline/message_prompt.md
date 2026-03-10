# MESSAGE PROMPT — Stage 3 (Outline)

## SECTION ONE — ENTRY POINT

SOURCE BUNDLE:

* `<REPOSITORY_ARCHIVE_NAME.zip>`

ENTRY POINT:

* `/README.md`

RULES:

* Используй только файлы внутри `<REPOSITORY_ARCHIVE_NAME.zip>`.
* Игнорируй любые отдельно загруженные файлы вне архива.
* Сначала прочитай `/README.md` и `docs/authoring/pipeline/PIPELINE.md`.
* Затем прочитай:
  * `docs/authoring/pipeline/stage_3_outline/README.md`
  * `docs/authoring/pipeline/stage_3_outline/stage_prompt.md`

---

## SECTION TWO — ACTIVE STAGE

ACTIVE STAGE:

* `Stage 3 — Outline`

STAGE FOLDER:

* `docs/authoring/pipeline/stage_3_outline/`

---

## SECTION THREE — TASK

BOARD ID:

* `<название платы / ревизия / дата>`

TASK:

* Подготовить `outline.md` как content contract и Figures Plan для полного Stage four.
* Обязательно подготовить `author_notes.md` как русский chapter-aligned project-specific опросник по собранному outline, а не как копию общего шаблона.
* Если planned figures / tables имеют owner = `author` или `mixed`, либо status = `to prepare` / `missing`, выдать `figure_brief_template.md`.
* После этого дать пользователю точный handoff: что он делает сейчас, куда кладёт файлы и что возвращает ассистенту.

OPTIONAL CONTEXT:

* figures_present: `<yes/no>`
* design_intent_present: `<yes/no>`
* author_notes_present: `<yes/no>`
* structure_priorities: `<list | none>`

---

## SECTION FOUR — EXPECTED RESULT

Return:

* `docs/authoring/artifacts/stage_3_outline/outline.md`
* `docs/authoring/artifacts/stage_3_outline/author_notes.md`
* if needed: `docs/authoring/artifacts/stage_3_outline/figure_brief_template.md`

And then a short mandatory handoff:

* `Stage status`
* `Assistant output`
* `Author action now`
* `Where to put it`
* `Next re-entry point`
* `Go / No-Go`
