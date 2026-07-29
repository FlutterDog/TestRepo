/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "LDO1118 Rev3 Firmware", "index.html", [
    [ "Назначение", "index.html#autotoc_md1", null ],
    [ "Основные параметры", "index.html#autotoc_md3", null ],
    [ "Состав проекта", "index.html#autotoc_md5", [
      [ "<span class=\"tt\">main.cpp</span>", "index.html#autotoc_md6", null ],
      [ "<span class=\"tt\">app/</span>", "index.html#autotoc_md7", null ],
      [ "<span class=\"tt\">app/ldo_board.hpp</span>, <span class=\"tt\">app/ldo_board.cpp</span>", "index.html#autotoc_md8", null ],
      [ "<span class=\"tt\">hal/</span>", "index.html#autotoc_md9", null ],
      [ "<span class=\"tt\">Libs/IGAS_mb.*</span>", "index.html#autotoc_md10", null ]
    ] ],
    [ "Аппаратная карта LDO", "index.html#autotoc_md12", [
      [ "Новая плата", "index.html#autotoc_md13", null ],
      [ "Старая плата", "index.html#autotoc_md14", null ]
    ] ],
    [ "Адресный переключатель", "index.html#autotoc_md16", null ],
    [ "RS-485", "index.html#autotoc_md18", null ],
    [ "Релейные выходы", "index.html#autotoc_md20", null ],
    [ "Карта Modbus-регистров", "index.html#autotoc_md22", [
      [ "HR 0 — управление релейными выходами", "index.html#autotoc_md23", null ],
      [ "HR 1 — состояние релейных выходов", "index.html#autotoc_md24", null ],
      [ "HR 3 — байт ошибки", "index.html#autotoc_md25", null ],
      [ "HR 580 — сервисный режим", "index.html#autotoc_md26", null ],
      [ "HR 590...593 — резерв", "index.html#autotoc_md27", null ],
      [ "HR 594 — программная перезагрузка", "index.html#autotoc_md28", null ],
      [ "HR 900 — переход в bootloader", "index.html#autotoc_md29", null ]
    ] ],
    [ "Поведение при потере связи", "index.html#autotoc_md31", null ],
    [ "Сборка прошивки", "index.html#autotoc_md33", [
      [ "Рекомендуемые параметры проекта", "index.html#autotoc_md34", null ],
      [ "Порядок сборки", "index.html#autotoc_md35", null ]
    ] ],
    [ "Генерация Doxygen-документации", "index.html#autotoc_md37", null ],
    [ "Прошивка микроконтроллера", "index.html#autotoc_md39", [
      [ "Прошивка через ISP", "index.html#autotoc_md40", null ],
      [ "Прошивка через bootloader", "index.html#autotoc_md41", null ]
    ] ],
    [ "Проверка после прошивки", "index.html#autotoc_md43", null ],
    [ "Минимальный тест Modbus", "index.html#autotoc_md45", null ],
    [ "Возможные ошибки", "index.html#autotoc_md47", null ],
    [ "Примечания по сопровождению", "index.html#autotoc_md49", null ],
    [ "Состав передаваемых материалов", "index.html#autotoc_md51", null ],
    [ "Пространства имен", "namespaces.html", [
      [ "Пространства имен", "namespaces.html", "namespaces_dup" ],
      [ "Члены пространств имен", "namespacemembers.html", [
        [ "Указатель", "namespacemembers.html", null ],
        [ "Функции", "namespacemembers_func.html", null ],
        [ "Переменные", "namespacemembers_vars.html", null ],
        [ "Определения типов", "namespacemembers_type.html", null ],
        [ "Перечисления", "namespacemembers_enum.html", null ],
        [ "Элементы перечислений", "namespacemembers_eval.html", null ]
      ] ]
    ] ],
    [ "Структуры данных", "annotated.html", [
      [ "Структуры данных", "annotated.html", "annotated_dup" ],
      [ "Алфавитный указатель структур данных", "classes.html", null ],
      [ "Поля структур", "functions.html", [
        [ "Указатель", "functions.html", null ],
        [ "Функции", "functions_func.html", null ],
        [ "Переменные", "functions_vars.html", null ]
      ] ]
    ] ],
    [ "Файлы", "files.html", [
      [ "Файлы", "files.html", "files_dup" ],
      [ "Список членов всех файлов", "globals.html", [
        [ "Указатель", "globals.html", null ],
        [ "Функции", "globals_func.html", null ],
        [ "Переменные", "globals_vars.html", null ],
        [ "Определения типов", "globals_type.html", null ],
        [ "Перечисления", "globals_enum.html", null ],
        [ "Элементы перечислений", "globals_eval.html", null ],
        [ "Макросы", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"_i_g_a_s__mb_8cpp.html",
"namespaceanonymous__namespace_02app_8cpp_03.html#ad0d176a94ca57c313fd08ef695128887"
];

var SYNCONMSG = 'нажмите на выключить для синхронизации панелей';
var SYNCOFFMSG = 'нажмите на включить для синхронизации панелей';
var LISTOFALLMEMBERS = 'Полный список членов класса';