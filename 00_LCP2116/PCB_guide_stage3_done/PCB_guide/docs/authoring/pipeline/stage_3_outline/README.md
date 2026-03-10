# Stage 3 — Outline (`outline.md` + `author_notes.md`)

## Purpose

Stage three — это **поворотная и интерактивная** стадия.

Здесь не только проектируется структура будущего релизного текста, но и подготавливается **следующий ход автора**:

* где нужны рисунки;
* где нужен фрагмент схемы;
* где нужна таблица;
* где полезны инженерные заметки автора;
* что можно извлечь автоматически;
* что должен подготовить автор руками.

Stage three не считается качественно завершённым, если ассистент выдал только `outline.md` и не объяснил, **что теперь должен сделать автор**.

---

## Inputs

### Обязательные

* `fact_pack.md`
* `glossary.md`
* `open_items.md`
* `reference_sources.md`
* `design_intent.md`
* `file_map.md`

### Допустимые дополнительные

* figure assets, если они уже есть в `docs/authoring/artifacts/IMG/`
* board-specific manuals и official references из `sources/extras/` и `sources/extras/references/`
* style exemplars из `docs/authoring/pipeline/style_references/`

---

## Procedure

### Шаг один. Построить outline как content contract

Ассистент строит `outline.md` и для каждой функциональной главы заранее предусматривает подпункты:

* назначение;
* аппаратная реализация;
* ключевые характеристики;
* возможности и сценарии применения;
* ограничения и последствия модификации.

### Шаг два. Встроить полный Figures Plan

Для каждой planned единицы указать:

* Figure ID;
* asset type: `figure` / `table` / `diagram` / `schematic excerpt`;
* purpose;
* source;
* how to generate;
* responsible party: `assistant` / `author` / `mixed`;
* target file;
* insert location;
* readiness status: `available` / `to extract` / `to prepare` / `missing`;
* placeholder caption / note;
* draft placeholder path in `IMG/`.

❗ Figures Plan должен быть пригоден не только для planning, но и для прямой вставки placeholder blocks в `draft_v0.md` на Stage four.

Если доступны style exemplars, ассистент должен использовать их именно на этом шаге для решения:

* где уместен block diagram;
* где нужен power tree;
* где лучше schematic excerpt;
* где полезнее table, чем prose;
* какой плотности explanatory inserts разумны для документа.

При этом ассистент не должен переносить factual claims из style exemplars в ваш проект.

### Шаг три. Ассистент обязан сгенерировать `author_notes.md`

`author_notes.md` генерируется **самим ассистентом** на основании уже собранного `outline.md`.

Автор не должен с нуля придумывать структуру notes-файла.

Файл должен быть:

* project-specific, а не копией общего шаблона;
* разбит по фактическим главам текущего outline;
* написан как русский опросник;
* пригоден для краткого инженерного заполнения;
* адаптирован под тип главы;
* синхронизирован с planned figures и tables.

Адаптация под тип главы означает, что для разных разделов задаются разные акценты. Например:

* для overview / introduction — роль платы, класс задач, позиционирование изделия;
* для architecture — главная структура, роли блоков, системные акценты;
* для processor / power / Ethernet / RS-485 — назначение, ключевые характеристики, сценарии применения, ограничения обещаний;
* для connectors / protection / PCB — практическая полезность, типовые ошибки, чувствительные зоны, что нельзя упрощать.

### Шаг четыре. При необходимости выдать `figure_brief_template.md`

Если в Figures Plan есть assets со статусом `to prepare` или `missing` и owner = `author` или `mixed`, ассистент обязан дополнительно выдать шаблон `figure_brief_template.md`.

### Шаг пять. Обязательный handoff автору

После выдачи артефактов ассистент обязан **прямо сказать автору**:

* какие именно главы надо заполнить в `author_notes.md`;
* какие figure-assets или tables должен подготовить автор;
* какие official manuals или datasheet ещё стоит догрузить;
* есть ли смысл догрузить style exemplars в `docs/authoring/pipeline/style_references/`;
* куда именно всё это положить;
* что нужно вернуть ассистенту для refresh Stage three или для перехода к Stage four.

---

## Когда автор вступает в работу

Правильный порядок такой:

* сначала ассистент выдаёт `outline.md`;
* затем ассистент выдаёт `author_notes.md` по тем же главам;
* затем автор заполняет `author_notes.md` и, если нужно, figure briefs или сами assets;
* затем автор возвращает обновлённый архив или подготовленные author-owned файлы;
* только после этого выполняется сильный Stage four.

---

## Что делать с official manuals / datasheet

Если на Stage three выяснилось, что их не хватает для живого текста, ассистент обязан сказать об этом **до перехода к Draft**.

Правильный путь:

* положить их в `sources/extras/references/`;
* обновить `reference_sources.md` и Stage two артефакты;
* затем продолжить Stage three / Stage four.

Если не хватает именно style exemplars для более уверенного Figures Plan, их нужно положить в:

* `docs/authoring/pipeline/style_references/`

И использовать только как ориентир композиции. Refresh factual base для этого не требуется.

---

## Outputs

### Обязательные артефакты

* `docs/authoring/artifacts/stage_3_outline/outline.md`
* `docs/authoring/artifacts/stage_3_outline/author_notes.md`

### Обязательный дополнительный артефакт при author-owned assets

* `docs/authoring/artifacts/stage_3_outline/figure_brief_template.md`

---

## DoD

Stage three завершён качественно, если:

* `outline.md` содержит полную структуру финального документа;
* у каждой главы есть content contract;
* Figures Plan встроен в `outline.md`;
* `author_notes.md` сгенерирован и синхронизирован с текущим outline;
* `author_notes.md` не является сырой копией шаблона, а адаптирован под конкретные главы и planned assets;
* для всех planned figures и tables понятны owner и status;
* ассистент выдал явный handoff автору;
* после handoff понятно, можно ли уже идти в Stage four или сначала нужен author pass.
