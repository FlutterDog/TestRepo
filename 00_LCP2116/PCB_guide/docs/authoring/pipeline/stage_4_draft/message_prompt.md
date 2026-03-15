# MESSAGE PROMPT — Stage 4 (Draft)

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
  * `docs/authoring/pipeline/stage_4_draft/README.md`
  * `docs/authoring/pipeline/stage_4_draft/stage_prompt.md`

---

## SECTION TWO — ACTIVE STAGE

ACTIVE STAGE:

* `Stage 4 — Draft`

STAGE FOLDER:

* `docs/authoring/pipeline/stage_4_draft/`

---

## SECTION THREE — TASK

BOARD ID:

* `<название платы / ревизия / дата>`

TASK:

* Подготовить `docs/authoring/artifacts/stage_4_draft/draft_v0.md` строго по `outline.md` и полному factual base проекта.
* Обязательно использовать: `fact_pack.md`, `glossary.md`, `open_items.md`, `reference_sources.md`, `design_intent.md`, `file_map.md`, `author_notes.md`, board implementation sources и official component / module references.
* Подготовить `docs/authoring/artifacts/stage_4_draft/stage_4_handoff.md` с editorial residue, missing assets, weak chapters и статусом перехода на Stage five.

OPTIONAL CONTEXT:

* figures_present: `<yes/no>`
* author_notes_present: `<yes/no>`
* reference_sources_present: `<yes/no>`
* draft_priorities: `<list | none>`
* allow_supporting_updates: `<yes/no>`

---

## SECTION FOUR — EXPECTED RESULT

If full stage, return:

* `draft_v0.md`;
* `stage_4_handoff.md`;
* if needed: supporting updates for `file_map.md`, `fact_pack.md`, `glossary.md` or `open_items.md`.

If planned figures / tables are not yet ready, insert clean asset reference blocks in the Draft at their intended locations without visible placeholder notes.

If large official manuals / reference manuals are used, take general component description and top-level characteristics only from the first three meaningful pages of the document. Go deeper only for targeted verification of a concrete already-relevant function, interface, limitation or parameter.