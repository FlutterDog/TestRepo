# PIPELINE (6 stages) — one-shot выпуск Board Guide / System Reference Guide уровня TI / ADI

Этот документ — **канон пайплайна**: стадии, quality gates, жизненный цикл артефактов, требования к выходному продукту и правила использования stage-specific `message_prompt.md`.

Подробные инструкции по каждой стадии находятся в:

* `docs/authoring/pipeline/stage_*/`

Отдельные документы по взаимодействию автора и ассистента:

* `docs/authoring/pipeline/START_HERE.md`
* `docs/authoring/pipeline/HELP.md`
* `docs/authoring/pipeline/REENTRY_PROMPTS.md`
* `docs/authoring/pipeline/style_references/README.md`

---

## ноль. Принципы

### ноль точка один. Release vs Authoring

* **Release** (`docs/release/`) — то, что отдаётся наружу: финальный `board_guide.md` и папка `IMG/`.
* **Authoring** (`docs/authoring/`) — внутренняя работа: артефакты стадий, трассируемость, QC, open items, черновики, reference sources, design intent и notes автора.

❗ В `docs/release/` не допускаются:

* FACT / ASSUMPTION / UNKNOWN как служебные маркеры;
* внутренние заметки;
* process-language;
* open items;
* TBD / заглушки.

### ноль точка два. Классы утверждений

* **Implemented fact** — подтверждено тем, что узел реально реализован на плате.
* **Component capability fact** — подтверждено официальным источником по компоненту / модулю.
* **Board-supported capability** — возможность компонента или подсистемы, которая не противоречит конкретной реализации платы.
* **Design intent** — замысел и инженерная роль узла, подтверждённые автором.
* **Unsupported / unverified capability** — чип это умеет, но для данной платы это не подтверждено и не должно попадать в релиз как реальная функция.

### ноль точка три. Классы источников

#### A. Board implementation sources

Подтверждают, что **реально реализовано на конкретной плате**:

* native CAD;
* schematic;
* PCB exports;
* BOM;
* Gerber / drill;
* stackup;
* board-specific manuals в `sources/extras/`.

#### B. Component and module reference sources

Подтверждают, что **умеют применённые компоненты и модули**:

* официальные datasheet;
* product brief;
* reference manual;
* официальные module manuals;
* официальные application notes.

Эти источники должны быть либо заморожены в `sources/extras/references/`, либо перечислены в `reference_sources.md`.

#### C. Author intent / design rationale

Подтверждают, **зачем узел введён и что должен понять клиент**:

* `design_intent.md`;
* `author_notes.md`;
* ответы автора по разделам.

### ноль точка четыре. FACT / ASSUMPTION / UNKNOWN

* **FACT** — подтверждено источником нужного класса с точным якорем.
* **ASSUMPTION** — допущение, явно зафиксировано и контролируется.
* **UNKNOWN** — неизвестно / не подтверждено; требуется отдельный источник или проверка.

### ноль точка пять. Главное правило релиза

Релизный документ должен быть:

* **честным по фактам**;
* **объясняющим по смыслу**;
* **полезным для инженера и интегратора**;
* но **не выглядеть как отчёт процесса**.

Если факт не подтверждён:

* он не должен маскироваться под уверенное утверждение;
* но и не должен превращаться в длинную оговорку внутри релизной прозы.

Неподтверждённое живёт в:

* `open_items.md`
* `qc_report.md`

А не в `docs/release/board_guide.md`.

## ноль точка шесть. Режим ассистент-ведёт-автора

Пайплайн должен работать не как пассивный набор шаблонов, а как **сценарий с явным handoff между ассистентом и автором**.

В конце **каждой** стадии ассистент обязан явно сообщить:

* завершена ли стадия или только подготовлен промежуточный проход;
* какие артефакты уже созданы;
* что именно теперь должен сделать автор;
* какие файлы автор должен заполнить / подготовить / догрузить;
* куда именно эти файлы нужно положить в репозитории;
* можно ли уже переходить на следующую стадию.

Ассистент не должен ждать, пока пользователь сам догадается спросить «что дальше».

### ноль точка шесть точка один. Формат обязательного handoff

После каждой стадии ассистент обязан выдать короткий, но точный handoff следующего вида:

* `Stage status` — `done` / `done with gaps` / `needs author action`;
* `Assistant output` — какие файлы уже подготовлены;
* `Author action now` — какие действия должен сделать автор прямо сейчас;
* `Where to put it` — путь в репозитории;
* `Next re-entry point` — что автор должен вернуть ассистенту для продолжения;
* `Go / No-Go` — можно ли уже идти дальше.

### ноль точка шесть точка два. Кто владеет следующем ходом

Если следующий необходимый артефакт принадлежит автору, ассистент обязан прямо сказать, что сейчас ход автора.

Если все обязательные author-owned действия выполнены, ассистент обязан прямо сказать, что теперь ход ассистента.

### ноль точка шесть точка три. Особое правило для Stage three

Stage three считается **интерактивной стадией в два прохода**:

* проход `A` — ассистент готовит `outline.md`, project-specific `author_notes.md` и figure / table planning;
* проход `B` — автор заполняет `author_notes.md` и при необходимости готовит figure briefs или сами assets;
* при необходимости после этого делается refresh Stage three перед переходом к Stage four.

Project-specific `author_notes.md` означает:

* он строится по фактическим главам текущего `outline.md`;
* он не должен быть механической копией общего шаблона;
* он должен адаптировать вопросы под тип главы: overview, architecture, processor, power, interfaces, connectors, protection, PCB, platform capabilities, limits;
* он должен учитывать planned figures и tables именно для текущего проекта.

Следовательно, завершением Stage three по умолчанию считается не только готовый `outline.md`, но и явный handoff автору.

### ноль точка шесть точка ноль. Робастный повторный вход

Если диалог длинный, контекст частично потерян или открыт новый чат, ассистент обязан опираться не на память переписки, а на:

* `PIPELINE.md`;
* `START_HERE.md`;
* `REENTRY_PROMPTS.md`;
* артефакты уже завершённых стадий внутри репозитория.

Автор в таком случае должен прислать новый актуальный архив и использовать один из готовых re-entry prompts.

### ноль точка шесть точка четыре. Особое правило для Stage four

Stage four допускается как рабочий, но уже читабельный draft.

Это означает:

* основной файл `draft_v0.md` должен читаться как нормальная пояснительная записка;
* ссылки на planned figures, tables и excerpts должны уже стоять в ожидаемых местах;
* отсутствие готовых image files не должно превращать основной текст в process-report;
* все editorial remarks, missing-asset notes, weak-chapter notes и рекомендации перед релизом должны выноситься в `stage_4_handoff.md`, а не внутрь `draft_v0.md`.

Недопустимо:

* открывать `draft_v0.md` служебным вступлением о состоянии стадии;
* вставлять в основной текст фразы вида «в текущем проходе», «перед Stage five», «нужно подготовить файл»;
* добавлять видимые note-строки про placeholders;
* превращать main draft в смесь документа и handoff-отчёта.

### ноль точка шесть точка пять. Режим help

Если автор загружает актуальный архив репозитория и пишет в чат `help`, ассистент обязан:

* показать краткую карту стадий;
* перечислить команды для старта и повторного входа;
* подсказать, как определить текущую стадию;
* указать, где лежат короткие и длинные инструкции в репозитории.

Ассистент не должен отвечать на `help` общими рассуждениями. Он должен отвечать как на **команду открытия краткого меню**.

Канонический справочник по этому режиму находится в:

* `docs/authoring/pipeline/HELP.md`

---

## один. Каноническая структура репозитория (layout)

ℹ️ Структура поставляется вместе с репозиторием. Скриптов инициализации структуры **нет**.

```text
/
├─ README.md
├─ .gitignore
│
├─ sources/
│  ├─ cad/
│  ├─ schematic/
│  ├─ pcb/
│  ├─ bom/
│  ├─ gerber/
│  ├─ stackup/
│  └─ extras/
│     └─ references/
│
├─ docs/
│  ├─ authoring/
│  │  ├─ pipeline/
│  │  │  ├─ PIPELINE.md
│  │  │  ├─ style_references/
│  │  │  ├─ stage_0_start/
│  │  │  ├─ stage_1_freeze_sources/
│  │  │  ├─ stage_2_fact_pack/
│  │  │  ├─ stage_3_outline/
│  │  │  ├─ stage_4_draft/
│  │  │  └─ stage_5_qc_release_build/
│  │  │
│  │  └─ artifacts/
│  │     ├─ IMG/
│  │     ├─ stage_1_freeze_sources/
│  │     ├─ stage_2_fact_pack/
│  │     ├─ stage_3_outline/
│  │     ├─ stage_4_draft/
│  │     └─ stage_5_qc_release_build/
│  │
│  └─ release/
│     └─ IMG/
```

### Что означает этот layout

Это template-layout репозитория до финальной релизной сборки.

### Канонический путь к изображениям

**Authoring images:**

* `docs/authoring/artifacts/IMG/`

**Release images:**

* `docs/release/IMG/`

❗ Если в старых документах встречается `docs/authoring/IMG/`, считать это устаревшим путём и не использовать.

---

## два. Зоны репозитория и жизненный цикл

### два точка один. `sources/` — Source of Truth

* **Путь:** `sources/`
* **Статус:** изменяем только на Stage one, далее — freeze
* **Правило:** после Stage one исходники не редактируются

Если состав исходников меняется, это означает новый freeze-комплект.

### два точка два. `docs/authoring/artifacts/` — рабочие артефакты

* **Путь:** `docs/authoring/artifacts/`
* **Статус:** активно редактируется на Stage one–Stage five
* **Правило:** каждый файл имеет окно жизни и назначение

### два точка три. `docs/authoring/pipeline/` — регламент

* **Путь:** `docs/authoring/pipeline/`
* **Статус:** почти статичен
* **Правило:** меняется только при улучшении процесса

### два точка четыре. `docs/release/` — релиз

* **Путь:** `docs/release/`
* **Статус:** собирается на Stage five
* **Состав в финальном состоянии:** `board_guide.md` + `IMG/`

---

## три. Pipeline overview (крупная клетка)

| Stage | Название | Основные выходы | Критерий готовности |
| ----- | -------- | --------------- | ------------------- |
| zero | Start | понятен объект документирования; структура репозитория готова | нет путаницы в путях и идентификаторе платы |
| one | Freeze Sources | `sources/*`; `release_manifest.md`; старт `file_map.md` | один комплект источников собран и заморожен |
| two | Fact Pack | `fact_pack.md`; `glossary.md`; `open_items.md`; `reference_sources.md`; `design_intent.md`; расширен `file_map.md` | факты, component references и design intent собраны и трассируемы |
| three | Outline | `outline.md`; `author_notes.md`; при необходимости `figure_brief_template.md` | структура документа, content contract, figures plan и project-specific notes зафиксированы |
| four | Draft | `draft_v0.md`; `stage_4_handoff.md` | получен reader-facing draft: текст читается как документ, покрывает outline, содержит figure links, но не содержит process-language и редакторских пометок внутри основного текста |
| five | QC + Release Build | `qc_report.md`; при необходимости `draft_v1.md`; `docs/release/board_guide.md`; `docs/release/IMG/` | релиз чистый, самодостаточный, объясняет плату как guide и не содержит служебных следов authoring-процесса |

---

## четыре. Артефакты и их жизненный цикл

### Stage one — Freeze Sources

#### `release_manifest.md`

**Путь:** `docs/authoring/artifacts/stage_1_freeze_sources/release_manifest.md`

Паспорт freeze-комплекта.

#### `file_map.md`

**Путь:** `docs/authoring/artifacts/stage_1_freeze_sources/file_map.md`

Стартует на Stage one и активно пополняется до Stage five.

### Stage two — Fact Pack

#### `fact_pack.md`

**Путь:** `docs/authoring/artifacts/stage_2_fact_pack/fact_pack.md`

Слой фактов по функциональным узлам.

#### `glossary.md`

**Путь:** `docs/authoring/artifacts/stage_2_fact_pack/glossary.md`

Единые термины, сокращения, единицы, naming rules.

#### `open_items.md`

**Путь:** `docs/authoring/artifacts/stage_2_fact_pack/open_items.md`

Централизованный список неподтверждённого.

#### `reference_sources.md`

**Путь:** `docs/authoring/artifacts/stage_2_fact_pack/reference_sources.md`

Список разрешённых официальных внешних источников по компонентам и модулям.

#### `design_intent.md`

**Путь:** `docs/authoring/artifacts/stage_2_fact_pack/design_intent.md`

Инженерный смысл узлов: зачем узел нужен, что он открывает, что нельзя ошибочно обещать.

### Stage three — Outline

#### `outline.md`

**Путь:** `docs/authoring/artifacts/stage_3_outline/outline.md`

Скелет финального документа, content contract и figures plan.

#### `author_notes.md`

**Путь:** `docs/authoring/artifacts/stage_3_outline/author_notes.md`

Обязательный project-specific слой author intent и инженерных комментариев по уже утверждённому outline.

Этот файл не является релизным, но обязан активно использоваться на Stage four и при необходимости на Stage five для усиления смысла, design rationale, ограничений и chapter-specific акцентов.

### Stage four — Draft

#### `draft_v0.md`

**Путь:** `docs/authoring/artifacts/stage_4_draft/draft_v0.md`

Первый полный reader-facing draft, уже читаемый как документ.

#### `stage_4_handoff.md`

**Путь:** `docs/authoring/artifacts/stage_4_draft/stage_4_handoff.md`

Служебный handoff Stage four: что ещё надо дочистить перед релизом, каких assets пока нет, где текст ещё слабоват, можно ли уже идти на Stage five или нужен ещё один author / editor pass.

Этот файл является authoring-артефактом и не должен смешиваться с `draft_v0.md`.

### Stage five — QC + Release Build

#### `qc_report.md`

**Путь:** `docs/authoring/artifacts/stage_5_qc_release_build/qc_report.md`

Результаты финальной проверки и release-сборки.

---

## пять. Что означает хороший текст в этом пайплайне

### Пять точка один. Недостаточно просто перечислить, что есть на плате

Плохой раздел — это короткий инвентарный список вида:

* что стоит;
* куда подключено;
* что нельзя менять.

### Пять точка два. Хороший раздел обязан раскрывать пять слоёв

Для каждого крупного узла или подсистемы целевой текст должен раскрывать:

* **назначение** — зачем узел существует;
* **реализацию** — на чём и как он собран;
* **ключевые характеристики** — только подтверждённые нужным классом источников;
* **возможности и сценарии применения** — что это даёт пользователю, интегратору, новому владельцу;
* **ограничения / инварианты** — что критично для изменений и валидации.

### Пять точка три. Какой стиль считается целевым

Целевой стиль — **средний режим между сухим board guide и system reference manual**:

* без маркетинга;
* без переписывания даташитов целиком;
* но с таблицами, характеристиками, архитектурными пояснениями и практическим смыслом.

---

## шесть. Правило по внешним источникам

Официальные внешние источники по компонентам и модулям в этом пайплайне являются не только proof-base, но и рабочим материалом для сильного текста.

Разрешено и нужно использовать:

* datasheet;
* reference manual;
* product brief;
* module manual;
* официальные application notes;

если соблюдены все условия:

* источник официальный;
* он относится к реально применённому компоненту, модулю или подсистеме;
* он перечислен в `reference_sources.md` и предпочтительно заморожен в `sources/extras/references/`;
* его использование не противоречит freeze-комплекту платы;
* описание из reference-материала привязано к реальной роли узла на данной плате.

Что именно разрешено брать из official references:

* краткое нейтральное описание класса компонента и его инженерной роли;
* архитектурные особенности, если они реально важны для данной платы;
* подтверждённые ключевые характеристики;
* режимы работы, интерфейсы, защитные механизмы, power / clock / reset / memory / communication features — но только в объёме, релевантном данной реализации платы;
* словарь формулировок для сильного explanatory prose.

### Правило для больших official manuals / reference manuals

Если официальный reference-документ по компоненту очень большой по объёму
(например, крупный ARM-процессор, SoC, PMIC или многостраничный module manual),
то для общего описания компонента и верхнеуровневых характеристик
нужно использовать только первые три содержательные страницы документа.

Под содержательными страницами понимаются страницы introduction / description /
overview / features summary, а не обложка, legal pages или table of contents.

Цель этого правила:

* брать сильное вводное техническое описание и summary-характеристики;
* не раздувать текст случайными deep-feature details;
* не превращать Board Guide в пересказ giant manual.

Глубже первых трёх содержательных страниц разрешено идти только для
точечной верификации конкретной уже релевантной функции, режима, интерфейса,
ограничения или характеристики, если это прямо нужно для описания
реально реализованной на плате схемы.

Что запрещено:

* копировать marketing language;
* вставлять в текст всю feature-list из datasheet без отбора;
* превращать capability компонента в capability платы без board-level привязки;
* подменять описание конкретной реализации платы общим пересказом reference manual;
* подтягивать характеристики по памяти или по неофициальным пересказам.

Правильный принцип такой:

* official component manuals и datasheets дают материал для сильных описательных абзацев;
* board sources подтверждают, что именно реально реализовано;
* author notes и design intent объясняют, зачем это было сделано и что важно понять читателю.

---

## семь. Правило по заметкам автора

На этом пайплайне считается, что Stage three обязан сформировать и передать дальше:

* `docs/authoring/artifacts/stage_3_outline/author_notes.md`

Этот файл обязателен и нужен для:

* design rationale по главам;
* живых инженерных объяснений;
* уточнения, что именно должен понять клиент или новый владелец;
* указания, что нельзя упростить, потерять или сформулировать слишком слабо;
* уточнения ограничений и практических нюансов, которых может не быть в board sources и reference manuals.

`author_notes.md` не является релизным файлом, но его нужно активно использовать на Stage four и при необходимости на Stage five для усиления слабых глав.

---

## восемь. Переходы между стадиями

### Переход Stage two → Stage three

Нужны:

* fact-layer;
* design intent;
* reference sources;
* open items;
* базовая трассируемость.

### Переход Stage three → Stage four

Нужны:

* complete outline;
* content contract по разделам;
* figures plan;
* `author_notes.md`;
* `reference_sources.md`, покрывающий применённые ключевые компоненты и модули;
* official references, доступные для активного использования в тексте.

Stage four не должен стартовать в режиме «author notes опциональны» или «manuals есть, но можно не использовать».

Если каких-то critical official references по ключевым компонентам не хватает, сначала нужно усилить factual base, а потом идти в Draft.

### Переход Stage four → Stage five

Нужны:

* reader-facing `draft_v0.md`, который покрывает весь outline;
* `stage_4_handoff.md` с остаточными editorial issues;
* figure links и captions уже стоят в тексте;
* в основном Draft нет process-language, status notes, author instructions и видимых служебных пометок;
* допускается отсутствие самих image files, если ссылки и места вставки уже зафиксированы корректно.

Stage five может выполняться повторно, пока не будет получен clean release.

---

## девять. Мини-QC для всего станка

Пайплайн считается настроенным правильно, если:

* Stage two собирает не только implemented facts, но и capabilities и design intent;
* Stage three заранее требует разделы «назначение / реализация / характеристики / возможности / ограничения»;
* Stage four не может законно выдать сухую инвентаризацию вместо инженерного текста;
* Stage five проверяет не только factual cleanliness, но и explanatory adequacy.

### ноль точка шесть. Кто ведёт процесс и кто даёт дополнительные материалы

* Ассистент обязан вести пользователя по стадиям и в конце каждой стадии явно говорить:
  * что уже готово;
  * что остаётся сделать;
  * что требуется от автора;
  * можно ли переходить дальше.
* Если на следующей стадии полезны действия автора, ассистент должен назвать их явно, а не ожидать молчаливого знания пайплайна.

### ноль точка семь. Жизненный цикл официальных reference-материалов

* Предпочтительный момент для добавления datasheet, module manual и reference manual — Stage one вместе с freeze-комплектом.
* Допустимо добавить их до начала Stage two как controlled supplement, если они:
  * официальные;
  * относятся к реально применённым компонентам / модулям;
  * помещены в `sources/extras/references/`;
  * перечислены затем в `reference_sources.md`.
* Если новые official references добавлены после завершения Stage two, Stage two должен быть обновлён, потому что изменился разрешённый factual base.

### ноль точка семь точка один. Жизненный цикл style exemplars

* Style exemplars лучше добавлять до конца Stage two или в начале Stage three.
* Их каноническое место — `docs/authoring/pipeline/style_references/`.
* Они нужны для выбора композиции документа: figures, tables, diagrams, depth of explanation и document rhythm.
* Они не должны смешиваться с factual references и не должны попадать в `reference_sources.md` как factual proof-source.
* Если author догружает style exemplars перед Stage three, ассистент обязан использовать их только как style-only input при построении `outline.md`, `Figures Plan`, `author_notes.md` и Draft structure.

### ноль точка восемь. Жизненный цикл иллюстраций, схем и таблиц

* Figures Plan появляется на Stage three внутри `outline.md`.
* На Stage three ассистент обязан не только перечислить будущие figures, но и классифицировать их:
  * уже доступны из текущего freeze-комплекта;
  * требуют извлечения / рендеринга из CAD / PDF;
  * будут подготовлены автором вручную или отдельно догружены.
* Для каждой planned figure должны быть понятны:
  * назначение;
  * источник;
  * ответственный за подготовку;
  * целевое имя файла в `IMG/`;
  * место вставки в тексте.

На Stage four:

* если asset уже существует, вставляется обычная рабочая markdown-ссылка;
* если asset ещё не готов, в основной Draft вставляется только clean asset reference block:
  * HTML comment с Figure ID, type, owner и status;
  * markdown image-link или table reference с целевым путём в `IMG/`;
  * без видимой служебной note-строки.

Все замечания вида:

* чего именно не хватает;
* кто должен подготовить asset;
* мешает ли отсутствие asset чтению главы;
* нужен ли дополнительный author pass;

должны уходить в `stage_4_handoff.md`, а не в основной текст Draft.

Если author позже кладёт файл с тем же именем в `IMG/`, markdown-ссылка в Draft или Release автоматически начинает указывать на реальный asset.

Таблицы подчиняются тем же правилам: либо чистая встроенная таблица, либо clean reference без видимой служебной пометки.

### ноль точка девять. Style exemplars и внешние образцы оформления

* TI / ADI / BeagleBoard manuals можно использовать как **style exemplars** и structural references.
* Они не являются factual source для вашей платы и не должны подменять board facts или component facts.
* Их роль — помочь выбрать типы разделов, плотность текста, набор figures, diagrams и tables.
* Если style exemplars используются в проекте, их лучше хранить отдельно от factual references, например в `docs/authoring/pipeline/style_references/` или иной явно выделенной style-only зоне.