# STAGE PROMPT — Stage 2 (Fact Pack)

Ты работаешь по канону репозитория:

* `docs/authoring/pipeline/PIPELINE.md`
* `/README.md`

Сейчас активная стадия: **Stage 2 — Fact Pack**.

## GOAL

Собрать и структурировать факты по плате так, чтобы:

* на их основе можно было зафиксировать структуру финального документа на Stage three;
* на Stage four можно было написать **не сухую инвентаризацию, а связный инженерный текст** без выдуманных чисел и спорных утверждений.

Основной результат Stage two:

* `fact_pack.md`
* `glossary.md`
* `open_items.md`
* `reference_sources.md`
* `design_intent.md`
* обновлённый `file_map.md`

---

## HARD RULES (не нарушать)

### Один. Scope: только hardware

### Два. Нулевая фантазия

Если для факта нет источника, это не FACT.

### Три. Различай классы утверждений

Не смешивай:

* implemented fact;
* component capability fact;
* board-supported capability;
* design intent.

### Четыре. `sources/*` не редактируются

Stage two работает только по freeze-комплекту Stage one.

### Пять. У каждого значимого факта должно быть «где смотреть»

### Шесть. Всё неподтверждённое обязано попасть в `open_items.md`

### Семь. Никакого превращения Stage two в Draft

### Восемь. Проверь достаточность reference-материалов

Если для содержательного Draft явно не хватает official component references, нужно прямо сказать об этом пользователю и перечислить, какие документы полезно догрузить в `sources/extras/references/`.

### Девять. Проверь наличие style exemplars

Если для будущего Outline / Figures Plan / Draft composition полезны style exemplars, прямо скажи об этом пользователю и перечисли, какие документы-образцы полезно положить в `docs/authoring/pipeline/style_references/`.

Ты обязан понимать, что style exemplars применяются только для:

* выбора плотности текста;
* выбора типов figures, tables, diagrams и schematic excerpts;
* выбора общей композиции документа.

Они не могут использоваться как factual proof-source.

---

## FACT PACK WRITING BAR

* Не только фиксируй реализацию платы, но и готовь сырьё для объясняющего текста.
* Отделяй возможности компонента от возможностей именно этой платы.
* Если author intent уже известен, зафиксируй его как отдельный слой.

---

## OUTPUT FORMAT

Выдай:

* `## docs/authoring/artifacts/stage_2_fact_pack/fact_pack.md`
* `## docs/authoring/artifacts/stage_2_fact_pack/glossary.md`
* `## docs/authoring/artifacts/stage_2_fact_pack/open_items.md`
* `## docs/authoring/artifacts/stage_2_fact_pack/reference_sources.md`
* `## docs/authoring/artifacts/stage_2_fact_pack/design_intent.md`

При необходимости дополнительно:

* обновлённый `file_map.md`

---

## QUALITY BAR

* Stage two должен подготовить не только factual correctness, но и достаточную factual base для живого Draft.
* В конце стадии нужно явно сказать, чего не хватает: facts, official references, style exemplars или author intent.
