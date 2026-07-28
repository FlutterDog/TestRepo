# Style References

Эта папка предназначена для **style exemplars**: внешних документов-образцов, которые помогают выбрать композицию будущего `board_guide.md`, но **не являются factual source** для вашей платы.

## Что сюда класть

Например:

* `TI_Board_Guide_Example.pdf`
* `ADI_System_Reference_Manual.pdf`
* `BeagleBoard_System_Reference_Manual.pdf`
* другие хорошо структурированные board / system reference manuals.

## Для чего они нужны

Ассистент использует их только как ориентир по:

* плотности текста;
* типам разделов и подглав;
* размещению figures, tables, diagrams и schematic excerpts;
* типовым explanatory inserts;
* балансу между сухим board guide и более живым system reference manual.

## Для чего они не нужны

Эти документы нельзя использовать как доказательство фактов по вашей плате.

Они не заменяют:

* schematic;
* BOM;
* native CAD;
* board-specific manuals;
* official component references в `sources/extras/references/`.

## Когда их добавлять

Лучше всего — до конца `Stage two` или в начале `Stage three`, чтобы ассистент мог использовать их при построении `outline.md`, `Figures Plan`, `author_notes.md` и будущего Draft.

## Как ассистент должен их применять

Ассистент обязан:

* не извлекать из них factual claims по вашей плате;
* использовать их только как style-only input;
* явно сообщать автору, если style exemplars помогли выбрать тип figures, tables или плотность композиции.
