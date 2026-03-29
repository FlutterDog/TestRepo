# Asset Reference Convention — Stage 4

Этот файл задаёт канонический вид clean asset reference blocks в `draft_v0.md`.

## Для figure / diagram / schematic excerpt

````md
<!-- FIGURE_PLACEHOLDER: F-ноль один | type=diagram | owner=author | status=to_prepare -->
![Структурная схема платы и основных функциональных блоков.](IMG/block_diagram.png)
````

## Для table asset

````md
<!-- TABLE_PLACEHOLDER: T-ноль один | owner=author | status=to_prepare -->
[Таблица T-ноль один: ключевые линии питания](IMG/power_table.md)
````

## Правила

* Reference block ставится в том месте Draft, где asset логически должен появиться.
* `target file` должен совпадать с Figures Plan.
* В основном Draft не используется видимая служебная note-строка про подготовку файла.
* Если author позже кладёт файл с тем же именем в `IMG/`, ссылка начинает указывать на реальный asset.
* Все remarks о missing assets, owner action и readiness живут в `stage_4_handoff.md`.
* Перед финальным релизом clean asset reference либо указывает на реальный asset, либо сохраняется как чистая ссылка без служебной prose, если проект допускает позднюю подкладку изображения.
