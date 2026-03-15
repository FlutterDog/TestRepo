# MESSAGE PROMPT — Stage 2 (Fact Pack)

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
  * `docs/authoring/pipeline/stage_2_fact_pack/README.md`
  * `docs/authoring/pipeline/stage_2_fact_pack/stage_prompt.md`

---

## SECTION TWO — ACTIVE STAGE

ACTIVE STAGE:

* `Stage 2 — Fact Pack`

STAGE FOLDER:

* `docs/authoring/pipeline/stage_2_fact_pack/`

---

## SECTION THREE — TASK

BOARD ID:

* `<название платы / ревизия / дата>`

TASK:

* Подготовить материалы Stage two:
  * `docs/authoring/artifacts/stage_2_fact_pack/fact_pack.md`;
  * `docs/authoring/artifacts/stage_2_fact_pack/glossary.md`;
  * `docs/authoring/artifacts/stage_2_fact_pack/open_items.md`;
  * `docs/authoring/artifacts/stage_2_fact_pack/reference_sources.md`;
  * `docs/authoring/artifacts/stage_2_fact_pack/design_intent.md`;
  * при необходимости обновить `docs/authoring/artifacts/stage_1_freeze_sources/file_map.md`.

FOCUS:

* `<full stage | exact scope>`

OPTIONAL CONTEXT:

* release_manifest_ready: `<yes/no>`
* file_map_ready: `<yes/no>`
* reference_docs_present: `<yes/no>`
* list_reference_docs: `<list | none>`
* author_clarifications_present: `<yes/no>`
* known_source_gaps: `<list | none>`
* known_conflicts: `<list | none>`

---

## SECTION FOUR — EXPECTED RESULT

If `FOCUS` is `full stage`, return:

* `fact_pack.md`;
* `glossary.md`;
* `open_items.md`;
* `reference_sources.md`;
* `design_intent.md`;
* if needed: updated `file_map.md`.
