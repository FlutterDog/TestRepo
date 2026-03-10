# Asset Placeholder Convention — Stage 4

Этот файл задаёт канонический вид placeholder blocks в `draft_v0.md`.

## Для figure / diagram / schematic excerpt

```md
<!-- FIGURE_PLACEHOLDER: F-ноль один | type=diagram | owner=author | status=to_prepare -->
![Структурная схема платы и основных функциональных блоков.](IMG/block_diagram.png)
_Запланированная иллюстрация: структурная схема платы и основных функциональных блоков. Подготовить файл `IMG/block_diagram.png`._
```

## Для table asset

```md
<!-- TABLE_PLACEHOLDER: T-ноль один | owner=author | status=to_prepare -->
[Таблица T-ноль один: ключевые линии питания](IMG/power_table.md)
_Запланированная таблица: ключевые линии питания. Подготовить файл `IMG/power_table.md` или встроить таблицу в текст перед релизом._
```

## Правила

* Placeholder ставится в том месте Draft, где asset логически должен появиться.
* `target file` должен совпадать с Figures Plan.
* `caption / note` должен кратко объяснять, что ожидается увидеть.
* Если author позже кладёт файл с тем же именем в `IMG/`, ссылка в Draft начинает указывать на реальный asset.
* Перед финальным релизом placeholder blocks должны быть либо закрыты реальными assets, либо осознанно удалены по QC-решению.
