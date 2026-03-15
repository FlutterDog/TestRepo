# MESSAGE PROMPT — Stage 1 (Freeze Sources)

## SECTION ONE — ENTRY POINT

SOURCE BUNDLE:

* `<REPOSITORY_ARCHIVE_NAME.zip>`
  // ЗАМЕНИ: имя актуального ZIP-архива репозитория.

ENTRY POINT:

* `/README.md`
  // НЕ МЕНЯТЬ.

RULES:

* Используй только файлы внутри `<REPOSITORY_ARCHIVE_NAME.zip>`.
* Игнорируй любые отдельно загруженные файлы вне архива.
* Сначала прочитай `/README.md` и `docs/authoring/pipeline/PIPELINE.md`.
* Затем прочитай:

  * `docs/authoring/pipeline/stage_1_freeze_sources/README.md`
  * `docs/authoring/pipeline/stage_1_freeze_sources/stage_prompt.md` — если файл существует.

---

## SECTION TWO — ACTIVE STAGE

ACTIVE STAGE:

* `Stage 1 — Freeze Sources`
  // НЕ МЕНЯТЬ.

STAGE FOLDER:

* `docs/authoring/pipeline/stage_1_freeze_sources/`
  // НЕ МЕНЯТЬ.

---

## SECTION THREE — TASK

BOARD ID:

* `<название платы / ревизия / дата>`
  // ЗАМЕНИ.

TASK:

* Выполнить Stage 1:

  * проверить комплект источников;
  * выявить признаки смешивания версий;
  * проверить или предложить раскладку по `sources/*`;
  * подготовить `docs/authoring/artifacts/stage_1_freeze_sources/release_manifest.md`;
  * подготовить старт `docs/authoring/artifacts/stage_1_freeze_sources/file_map.md`.

FOCUS:

* `<full stage | exact scope>`
  // ЗАМЕНИ ОБЯЗАТЕЛЬНО.

DELIVERY:

* `markdown + parts`
  // БАЗОВЫЙ РЕЖИМ.

OPTIONAL OVERRIDES:

* `<none | local restrictions>`
  // ЗАМЕНИ.

OPTIONAL CONTEXT:

* sources_already_structured: `<yes/no>`
* tree_available: `<yes/no>`
* missing_groups_known: `<list | none>`
* preferred_file_map_topics: `<list | none>`
* proof_for_FACT_available: `<yes/no>`
  // ЗАМЕНИ.

---

## SECTION FOUR — EXPECTED RESULT

If `FOCUS` is `full stage`, return:

* комплектная оценка freeze-комплекта;
* раскладка по `sources/*` или подтверждение, что она уже корректна;
* `docs/authoring/artifacts/stage_1_freeze_sources/release_manifest.md`;
* `docs/authoring/artifacts/stage_1_freeze_sources/file_map.md`.

If `FOCUS` is partial, stay strictly inside the requested Stage 1 scope.
