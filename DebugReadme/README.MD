PIPELINE (6 stages) — one-shot выпуск Board Guide уровня TI / ADI

Этот документ — канон пайплайна: стадии, quality gates, жизненный цикл артефактов, требования к выходному продукту и правила использования stage-specific message_prompt.md.

Подробные инструкции по каждой стадии находятся в:

docs/authoring/pipeline/stage_*/

0. Принципы
0.1 Release vs Authoring

Release (docs/release/) — то, что отдаётся наружу: финальный board_guide.md и папка IMG/.

Authoring (docs/authoring/) — внутренняя работа: артефакты стадий, трассируемость, QC, open items, черновики.

❗ В docs/release/ не допускаются:

FACT / ASSUMPTION / UNKNOWN;

внутренние заметки;

process-language;

open items;

TBD / заглушки.

0.2 FACT / ASSUMPTION / UNKNOWN

FACT — подтверждено источником (файл + якорь: лист / страница / RefDes / сеть / токен).

ASSUMPTION — допущение, явно зафиксировано и контролируется.

UNKNOWN — неизвестно / не подтверждено; требуется отдельный источник или проверка.

0.3 Главное правило релиза

Релизный документ должен быть:

честным по фактам,

но не выглядеть как отчёт процесса.

Если факт не подтверждён:

он не должен маскироваться под уверенное утверждение;

но и не должен превращаться в длинную оговорку внутри релизной прозы.

Неподтверждённое живёт в:

open_items.md

qc_report.md

А не в docs/release/board_guide.md.

1. Каноническая структура репозитория (layout)

ℹ️ Структура поставляется вместе с репозиторием (clone-first). Скриптов инициализации структуры нет.

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
│
├─ docs/
│  ├─ authoring/
│  │  ├─ pipeline/
│  │  │  ├─ PIPELINE.md
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
Что означает этот layout

Это template-layout репозитория до финальной релизной сборки.

То есть здесь показано:

что должно существовать заранее как каркас;

какие папки являются каноническими;

где живут источники, authoring-артефакты и будущая release-зона.

docs/release/board_guide.md не обязан существовать заранее в template-layout.
Он появляется только на Stage 5 как результат релизной сборки.

Канонический путь к изображениям

Authoring images:

docs/authoring/artifacts/IMG/

Release images:

docs/release/IMG/

❗ Если в старых документах встречается docs/authoring/IMG/, считать это устаревшим путём и не использовать.

Правило ссылок на изображения в релизе

В docs/release/board_guide.md использовать только пути вида:

IMG/<file>

2. Зоны репозитория и жизненный цикл
2.1 sources/ — Source of Truth

Путь: sources/

Статус: изменяем только на Stage 1, далее — freeze

Правило: после Stage 1 исходники не редактируются

Если состав исходников меняется:

это фиксируется через release_manifest.md;

и по сути означает новый freeze-комплект.

2.2 docs/authoring/artifacts/ — рабочие артефакты

Путь: docs/authoring/artifacts/

Статус: активно редактируется на Stage 1–5

Правило: каждый файл имеет окно жизни и назначение

2.3 docs/authoring/pipeline/ — регламент

Путь: docs/authoring/pipeline/

Статус: почти статичен

Правило: меняется только при улучшении процесса

2.4 docs/release/ — релиз

Путь: docs/release/

Статус: собирается на Stage 5

Состав в финальном состоянии: board_guide.md + IMG/

До Stage 5 папка docs/release/ является только местом будущей релизной сборки и не обязана заранее содержать board_guide.md.

Release — не место для внутренних пояснений, process-language и open items.

3. Pipeline overview (крупная клетка)
Stage	Название	Основные выходы	Критерий готовности
0	Start	понятен объект документирования; структура репозитория готова	нет путаницы в путях и идентификаторе платы
1	Freeze Sources	sources/* заполнен; release_manifest.md; старт file_map.md	один комплект источников собран и заморожен
2	Fact Pack	fact_pack.md; glossary.md; open_items.md; расширен file_map.md	факты по блокам собраны и трассируемы
3	Outline	outline.md	структура финального документа и figures plan зафиксированы
4	Draft	draft_v0.md (и при необходимости draft_v1.md)	полный связный черновик без TBD и выдуманных чисел
5	QC + Release Build	qc_report.md; docs/release/board_guide.md; docs/release/IMG/	релиз самодостаточен, чист и читается как guide
4. Артефакты и их жизненный цикл

Каноническая зона рабочих артефактов:

docs/authoring/artifacts/

Stage 1 — Freeze Sources
release_manifest.md

Путь: docs/authoring/artifacts/stage_1_freeze_sources/release_manifest.md

создаётся на Stage 1;

обновляется только при изменении состава sources/;

описывает, что именно входит в SoT.

file_map.md

Путь: docs/authoring/artifacts/stage_1_freeze_sources/file_map.md

стартует на Stage 1;

активно пополняется на Stage 2–5;

является каноническим слоем трассируемости.

Stage 2 — Fact Pack
fact_pack.md

Путь: docs/authoring/artifacts/stage_2_fact_pack/fact_pack.md

создаётся на Stage 2;

может уточняться на Stage 2–4;

это слой фактов по функциональным узлам, а не реестр компонентов.

glossary.md

Путь: docs/authoring/artifacts/stage_2_fact_pack/glossary.md

создаётся на Stage 2;

может обновляться до Stage 5;

задаёт единые термины, сокращения, единицы и стиль обозначений.

open_items.md

Путь: docs/authoring/artifacts/stage_2_fact_pack/open_items.md

создаётся на Stage 2;

ведётся до Stage 5;

содержит всё, что не подтверждено и не должно бездумно попадать в релиз.

❗ Правило: всё неподтверждённое должно либо жить здесь, либо быть удалено из релиза.

Stage 3 — Outline
outline.md

Путь: docs/authoring/artifacts/stage_3_outline/outline.md

создаётся на Stage 3;

может уточняться на Stage 3–4;

задаёт скелет финального документа и figures plan.

Stage 4 — Draft
draft_v0.md

Путь: docs/authoring/artifacts/stage_4_draft/draft_v0.md

создаётся на Stage 4;

является первым полным связным черновиком.

draft_v1.md

Путь: docs/authoring/artifacts/stage_4_draft/draft_v1.md

опционально создаётся на Stage 5;

используется, если после QC потребовались заметные нормализации текста.

Stage 5 — QC + Release Build
qc_report.md

Путь: docs/authoring/artifacts/stage_5_qc_release_build/qc_report.md

создаётся на Stage 5;

фиксирует, что проверено, что исправлено и что не вошло в релиз.

5. Quality gates (минимальные критерии «готово»)
5.1 Нет выдуманных чисел

Уровни, диапазоны, номиналы, частоты, pinout, stackup-параметры и прочие жёсткие факты:

либо подтверждаются;

либо не попадают в релиз.

5.2 Трассируемость

Ключевые утверждения должны быть проверяемы через:

file_map.md

и/или прямой якорь в sources/*

5.3 Консистентность

Термины, домены, единицы и обозначения должны быть единообразны:

в Draft;

в Release;

в glossary.md.

5.4 Release cleanliness

docs/release/board_guide.md должен быть:

без служебных статусов;

без внутренних замечаний;

без process-language;

без открытых вопросов в теле документа.

5.5 Качество инженерной прозы

Релиз должен читаться как плотный инженерный guide, а не как:

чеклист;

процессный отчёт;

набор оговорок;

машинный список утверждений.

Если раздел технически честен, но написан как «полупромт с ограничениями», он не проходит quality gate.

6. Release writing standard (обязательный стандарт релиза)
6.1 Что такое хороший релизный текст

Хороший релизный текст:

объясняет, что это за узел;

описывает, как он реализован;

фиксирует инварианты и практические следствия;

при необходимости даёт краткий bring-up / check.

6.2 Что нельзя писать в релизе

Запрещены формулировки вида:

«в данном релизе приводится только если...»

«это не подтверждено»

«нужно закрыть»

«см. open items»

«RefDes → net → разъём / пин» как объяснение процесса доказательства

Если факт не подтверждён:

не писать про него в релизе вообще;

оставить его только в open_items.md и qc_report.md.

6.3 Типовая форма хорошего раздела

Для каждого инженерного узла рекомендуемая структура:

одна короткая строка: что это;

один–два абзаца: как реализовано;

короткий подпункт: инварианты;

короткий подпункт: проверка / bring-up, если подтверждаемо.

Короткие списки допустимы. Длинные реестры RefDes — нет.

7. Expected structure of final Board Guide (TI-style)

Целевой файл:

docs/release/board_guide.md

Типовой каркас:

Назначение документа и область применения

Исходные данные и Source of Truth

Термины, сокращения и правила обозначений

Краткое описание платы / system overview

Электрическая схема по функциональным узлам

PCB / manufacturing notes (только подтверждённое)

Элементная база и critical parts

Bring-up / базовая проверка

Границы допустимых изменений (если это подтверждённый и полезный раздел)

Приложения (таблицы, pinout, power tree, block diagram, если доступны)

Важное правило по «открытым вопросам»

Раздел “Открытые вопросы / TBD” не является обязательной частью релиза.

Если open items не закрыты:

они должны жить в open_items.md и qc_report.md;

а не в docs/release/board_guide.md.

Исключение возможно только если ТЗ прямо требует отдельный раздел ограничений комплекта исходников. В таком случае это должен быть короткий, инженерный, не-процессный раздел, а не dump из open items.

8. Как использовать stage-specific message_prompt.md

ℹ️ Ниже — правило использования message_prompt.md активной стадии. Это лучше хранить именно в PIPELINE.md, а не в корневом README, потому что это часть процесса работы по стадиям, а не описание самого репозитория.

8.1 Каноническая точка входа в стадию

У каждой рабочей стадии каноническая точка входа — это:

docs/authoring/pipeline/stage_*/message_prompt.md

Именно этот файл считается dispatcher для соответствующей стадии.

Отдельного корневого dispatcher-файла в репозитории не предполагается.

Логика использования такая:

Пользователь открывает /README.md.

Затем читает docs/authoring/pipeline/PIPELINE.md.

После этого переходит в папку активной стадии.

Для запуска работы использует message_prompt.md этой стадии.

Ассистент читает:

/README.md

docs/authoring/pipeline/PIPELINE.md

README.md активной стадии

stage_prompt.md активной стадии, если он существует

После этого ассистент выполняет локальную задачу, указанную в stage-specific message_prompt.md.

8.2 Роль stage-specific message_prompt.md

message_prompt.md стадии нужен не для пересказа всей стадии, а для трёх вещей:

указать source bundle / источник;

указать активную стадию;

поставить локальную задачу внутри этой стадии.

То есть message_prompt.md — это dispatcher именно данной стадии, а не общий справочник по пайплайну.

8.3 Что должно быть в message_prompt.md

Stage-specific message_prompt.md должен содержать только минимальный набор:

Entry point — какой архив, snapshot или repository bundle считать Source of Truth;

Active stage — какая стадия сейчас выполняется;

Task — что именно нужно сделать на этой стадии сейчас.

Он не должен дублировать:

полное описание стадии;

полный procedural guide из stage README;

hard rules и quality bar из stage_prompt.md.

8.4 Где живёт объяснение, а где — шаблон

Разделение ролей такое:

PIPELINE.md — объясняет правило использования stage-specific message_prompt.md;

stage_*/message_prompt.md — готовый copy-paste шаблон для запуска конкретной стадии;

stage_*/README.md — procedural guide стадии;

stage_*/stage_prompt.md — рабочий режим, hard rules, quality bar и writing bar стадии.

Иными словами:

общее правило использования — в PIPELINE.md;

dispatcher конкретной стадии — в stage_*/message_prompt.md.

8.5 Что хранить в stage-папке

В папке конкретной стадии должен лежать готовый message_prompt.md, который:

уже привязан к этой стадии;

содержит минимальные комментарии для пользователя;

ясно показывает, какие строки менять перед отправкой.

Это и есть канонический dispatcher для данной стадии.

9. Standard: оформление stage-папок

Каждая stage-папка содержит минимум:

README.md

stage_prompt.md (опционально, но желательно)

message_prompt.md (обязателен для рабочих стадий; для Stage 0 не требуется)

Стандарт README.md

Purpose

Inputs (с точными путями)

Procedure

Outputs

DoD

Common mistakes

Example prompts (опционально)

Роль stage prompt

stage_prompt.md должен не просто повторять README стадии, а задавать:

рабочий режим генерации;

границы и hard rules;

quality bar именно для текста / артефакта стадии.

Если стадия влияет на стиль релизного документа (особенно Stage 4 и Stage 5), stage prompt обязан явно задавать и writing bar, а не только procedural checks.

10. Практический принцип использования пайплайна

Пайплайн задуман так, чтобы:

authoring был честным, подробным и проверяемым;

release был чистым, плотным и инженерно полезным.

Если возникает конфликт между:

«быть абсолютно честным по процессу»

и «писать хороший релизный текст»

правильное решение такое:

честность живёт в authoring;

качество инженерной прозы — в release.
