# Stage 2 — Fact Pack (`fact_pack.md` + `glossary.md` + `open_items.md` + `reference_sources.md` + `design_intent.md`)

## Purpose

Stage two создаёт **фактический, reference и смысловой слой**, на котором дальше строятся:

* структура документа на Stage three;
* содержательный инженерный текст на Stage four;
* чистый релиз на Stage five.

Стадия не только собирает факты по реализации платы, но и готовит **разрешённую factual base** по компонентам и модулям.

---

## Что может потребоваться от автора до старта Stage two

Автору полезно проверить, хватает ли официальных reference-материалов по ключевым узлам.

Обычно сюда входят:

* datasheet процессора / процессорного модуля;
* datasheet Ethernet-контроллеров;
* datasheet драйверов `RS-485`;
* module manual;
* product brief или reference manual по ключевым микросхемам.

Если этих материалов не хватает, их нужно положить в:

* `sources/extras/references/`

Это нужно сделать **до полноценного выполнения Stage two** или затем повторно обновить Stage two.

Если у автора есть хорошие документы-образцы по стилю и композиции будущего документа, их тоже можно заранее положить в:

* `docs/authoring/pipeline/style_references/`

Они не усиливают factual base, но помогают Stage three и Stage four выбрать нужные figures, tables и плотность текста.

---

## Inputs

### Обязательные входы

* `sources/*` заполнены и заморожены;
* `docs/authoring/artifacts/stage_1_freeze_sources/release_manifest.md`;
* `docs/authoring/artifacts/stage_1_freeze_sources/file_map.md`.

### Допустимые дополнительные входы

* board-specific manuals в `sources/extras/`;
* официальные component / module references в `sources/extras/references/`;
* style exemplars в `docs/authoring/pipeline/style_references/`;
* пояснения автора по design intent;
* точечные ответы автора по конфликтам BOM / schematic / manual.

---

## Procedure

### Шаг один. Проверить, хватает ли official references

Ассистент должен первым делом сказать:

* хватает ли уже загруженных reference-docs;
* каких именно official references не хватает для более живого текста на Stage four;
* нужно ли автору что-то догрузить до конца Stage two.
* есть ли style exemplars, которые полезны для будущей композиции документа.

### Шаг один точка один. Если reference docs неполны

Ассистент обязан прямо выдать author handoff:

* какие именно PDF / manuals / datasheet стоит догрузить;
* почему они нужны;
* куда именно их положить: `sources/extras/references/`;
* что после их загрузки нужен refresh Stage two.

Если для будущего Outline и Draft полезны style exemplars, ассистент должен отдельно сказать:

* какие именно документы-образцы было бы полезно добавить;
* что их нужно класть в `docs/authoring/pipeline/style_references/`;
* что они будут использоваться только для document composition, а не для factual claims.

### Шаг два. Собрать implemented facts

Для каждого крупного узла собрать:

* что это за узел;
* как реализован;
* где подтверждается;
* какие электрические инварианты видны по плате и схеме.

### Шаг три. Собрать component capability facts

Для ключевых компонентов и модулей собрать базовые характеристики, которые реально важны для будущего текста:

* скорость;
* разрядность;
* частоты;
* память;
* поддерживаемые режимы;
* значимые интерфейсы.

### Шаг четыре. Сформировать board-supported capabilities

Отдельно зафиксировать не только то, что умеет чип, но и то, что поддерживается конкретной платой без противоречия её реализации.

### Шаг пять. Собрать design intent

Для каждого крупного узла зафиксировать:

* зачем он нужен;
* какую роль играет в изделии;
* что он открывает пользователю или интегратору;
* что нельзя некорректно обещать клиенту.

### Шаг шесть. Сформировать `reference_sources.md`

### Шаг шесть точка один. Отметить доступность style exemplars

Если в проекте есть style exemplars, ассистент должен явно сказать в конце стадии:

* есть ли они уже в `docs/authoring/pipeline/style_references/`;
* достаточно ли их для уверенного Figures Plan и Draft composition;
* нужно ли автору что-то догрузить до начала Stage three.


Перечислить разрешённые внешние официальные источники, которые допустимо использовать на следующих стадиях.

### Шаг семь. Сформировать project glossary

Глоссарий должен содержать только реально используемые термины.

### Шаг восемь. Оформить `open_items.md`

Все пробелы, конфликты и спорные места вывести в `open_items.md`.

### Шаг девять. Обновить `file_map.md`

`file_map.md` должен отражать ключевые утверждения всех следующих классов:

* implemented fact;
* component capability fact;
* board-supported capability;
* design intent reference.

---

## Что ассистент должен сказать в конце Stage two

Ассистент обязан явно сообщить:

* достаточно ли factual base для перехода на Stage three;
* хватает ли official references для живого Draft;
* есть ли style exemplars для выбора композиции Figures / tables / diagrams;
* нужно ли автору ещё догрузить datasheet / manuals до начала Stage three;
* какие именно файлы автор должен загрузить прямо сейчас, если factual base ещё слабая;
* можно ли уже переходить дальше или сначала нужен author pass.

---

## Outputs

### Основные артефакты

* `docs/authoring/artifacts/stage_2_fact_pack/fact_pack.md`
* `docs/authoring/artifacts/stage_2_fact_pack/glossary.md`
* `docs/authoring/artifacts/stage_2_fact_pack/open_items.md`
* `docs/authoring/artifacts/stage_2_fact_pack/reference_sources.md`
* `docs/authoring/artifacts/stage_2_fact_pack/design_intent.md`

### Сопутствующее обновление

* `docs/authoring/artifacts/stage_1_freeze_sources/file_map.md`

---

## DoD

Stage two завершён, если:

* факты по ключевым узлам собраны;
* характеристики ключевых компонентов и модулей собраны;
* понятно, какие возможности поддерживаются на уровне платы;
* design intent зафиксирован хотя бы на уровне основных подсистем;
* все внешние официальные источники перечислены в `reference_sources.md`;
* все пробелы и конфликты вынесены в `open_items.md`;
* в конце стадии явно понятно, достаточно ли factual base для качественного Stage three и Stage four.
