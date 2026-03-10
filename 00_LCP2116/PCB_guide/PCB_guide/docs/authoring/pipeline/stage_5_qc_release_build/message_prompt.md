# MESSAGE PROMPT — Stage 5 (QC + Release Build)

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
  * `docs/authoring/pipeline/stage_5_qc_release_build/README.md`
  * `docs/authoring/pipeline/stage_5_qc_release_build/stage_prompt.md`

---

## SECTION TWO — ACTIVE STAGE

ACTIVE STAGE:

* `Stage 5 — QC + Release Build`

STAGE FOLDER:

* `docs/authoring/pipeline/stage_5_qc_release_build/`

---

## SECTION THREE — TASK

BOARD ID:

* `<название платы / ревизия / дата>`

TASK MODE:

* `<full stage | qc only | release build only | partial scope>`

OPTIONAL CONTEXT:

* base_draft: `<draft_v0.md | draft_v1.md>`
* IMG_files_present: `<yes/no>`
* author_notes_present: `<yes/no>`
* release_priorities: `<list | none>`
* explanatory_qc_strict: `<yes/no>`

---

## SECTION FOUR — EXPECTED RESULT

If `TASK MODE` is `full stage`, return:

* `qc_report.md`;
* `board_guide.md`;
* release image set;
* if needed: `draft_v1.md`.
