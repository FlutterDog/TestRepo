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

Если Stage three запланировал author-owned figures, tables, excerpts или author notes, а автор их ещё не подготовил, Stage four не должен тихо делать вид, что всё готово.

Ассистент обязан либо:

* прямо попросить отсутствующие артефакты до усиленного Draft;
* либо явно зафиксировать, что пользователь осознанно идёт дальше без них.

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
| two | Fact Pack | `fact_pack.md`; `glossary.md`; `open_items.md`; `reference_sources.md`; `design_intent.md`; расширен `file_map.md` | факты, возможности и design intent собраны и трассируемы |
| three | Outline | `outline.md`; optional `author_notes.md` | структура документа, content contract и figure plan зафиксированы |
| four | Draft | `draft_v0.md` | полный объясняющий черновик без TBD и выдуманных чисел |
| five | QC + Release Build | `qc_report.md`; `docs/release/board_guide.md`; `docs/release/IMG/` | релиз самодостаточен, объясняет плату и читается как guide |

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

#### `author_notes.md` (optional, strongly recommended)

**Путь:** `docs/authoring/artifacts/stage_3_outline/author_notes.md`

Глава-за-главой инженерные заметки автора по уже утверждённому outline.

### Stage four — Draft

#### `draft_v0.md`

**Путь:** `docs/authoring/artifacts/stage_4_draft/draft_v0.md`

Первый полный объясняющий черновик.

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

Официальные внешние источники разрешены, если соблюдены все условия:

* источник официальный;
* он перечислен в `reference_sources.md` и желательно заморожен в `sources/extras/references/`;
* он не противоречит freeze-комплекту платы;
* его использование не превращает релиз в компиляцию даташитов.

Запрещено:

* подтягивать характеристики по памяти;
* ссылаться на неформальные вторичные пересказы;
* писать capability платы, если подтверждена только capability чипа, но не проверена совместимость с конкретной реализацией платы.

---

## семь. Правило по заметкам автора

После Stage three автор может добавить:

* `docs/authoring/artifacts/stage_3_outline/author_notes.md`

Этот файл нужен для:

* design rationale по главам;
* живых инженерных объяснений;
* уточнения, что именно должен понять клиент;
* указания, что нельзя упростить или потерять в тексте.

`author_notes.md` **не** является релизным файлом, но его разрешено активно использовать на Stage four.

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
* при возможности — author notes.

### Переход Stage four → Stage five

Нужен:

* не просто полный, а **объясняющий** draft;
* без structural gaps;
* без сухих глав, сведённых к трём предложениям.

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
  * должны быть подготовлены автором вручную или отдельно загружены.
* Для каждой planned figure должны быть понятны:
  * назначение;
  * источник;
  * ответственный за подготовку;
  * момент, когда figure должна появиться в `docs/authoring/artifacts/IMG/`;
  * целевое имя файла в `IMG/`;
  * draft placeholder, который будет вставлен в `draft_v0.md`.
* На Stage four для planned, но ещё не готовых assets, ассистент обязан вставлять в Draft **placeholder blocks** по каноническому соглашению. Это нужно, чтобы автор видел точное место вставки, ожидаемое имя файла и назначение будущего asset.
* Если author позже кладёт файл с тем же именем в `IMG/`, markdown-ссылка в Draft автоматически начинает указывать на реальный asset.
* Таблицы подчиняются тем же правилам, что и figures. Если таблица нужна, но пока отсутствует, она должна быть явно зафиксирована в Figures Plan как table asset и получить draft placeholder.

### ноль точка девять. Style exemplars и внешние образцы оформления

* TI / ADI / BeagleBoard manuals можно использовать как **style exemplars** и structural references.
* Они не являются factual source для вашей платы и не должны подменять board facts или component facts.
* Их роль — помочь выбрать типы разделов, плотность текста, набор figures, diagrams и tables.
* Если style exemplars используются в проекте, их лучше хранить отдельно от factual references, например в `docs/authoring/pipeline/style_references/` или иной явно выделенной style-only зоне.
