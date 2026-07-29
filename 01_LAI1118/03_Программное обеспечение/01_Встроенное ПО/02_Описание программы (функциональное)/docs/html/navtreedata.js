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
  [ "LAI1118 Rev2 Firmware", "index.html", [
    [ "Назначение", "index.html#autotoc_md1", null ],
    [ "Основные параметры", "index.html#autotoc_md3", null ],
    [ "Состав проекта", "index.html#autotoc_md5", [
      [ "<span class=\"tt\">main.cpp</span>", "index.html#autotoc_md6", null ],
      [ "<span class=\"tt\">app/</span>", "index.html#autotoc_md7", null ],
      [ "<span class=\"tt\">app/lai_board.hpp</span>, <span class=\"tt\">app/lai_board.cpp</span>", "index.html#autotoc_md8", null ],
      [ "<span class=\"tt\">app/ads1115.hpp</span>, <span class=\"tt\">app/ads1115.cpp</span>", "index.html#autotoc_md9", null ],
      [ "<span class=\"tt\">hal/</span>", "index.html#autotoc_md10", null ],
      [ "<span class=\"tt\">Libs/IGAS_mb.*</span>", "index.html#autotoc_md11", null ]
    ] ],
    [ "Аппаратная карта LAI", "index.html#autotoc_md13", null ],
    [ "Адресный переключатель", "index.html#autotoc_md15", null ],
    [ "RS-485", "index.html#autotoc_md17", null ],
    [ "Аналоговые входы", "index.html#autotoc_md19", null ],
    [ "Калибровка аналоговых каналов", "index.html#autotoc_md21", null ],
    [ "Регистр индикации и регистр режимов входов", "index.html#autotoc_md23", null ],
    [ "EEPROM-карта", "index.html#autotoc_md25", null ],
    [ "Карта Modbus-регистров", "index.html#autotoc_md27", [
      [ "HR 0 и HR 600 — битовая маска состояния аналоговых входов", "index.html#autotoc_md28", null ],
      [ "HR 1...16 — калиброванные значения аналоговых каналов", "index.html#autotoc_md29", null ],
      [ "HR 39 — серийный номер", "index.html#autotoc_md30", null ],
      [ "HR 40 — версия ПО", "index.html#autotoc_md31", null ],
      [ "HR 51...66 — максимальные значения каналов", "index.html#autotoc_md32", null ],
      [ "HR 90 — сохранение калибровок", "index.html#autotoc_md33", null ],
      [ "HR 100 — Modbus-адрес", "index.html#autotoc_md34", null ],
      [ "HR 101...102 — порог фиксации максимального значения", "index.html#autotoc_md35", null ],
      [ "HR 115 — режим аналоговых входов", "index.html#autotoc_md36", null ],
      [ "HR 401...416 и HR 501...516 — изменение калибровочных точек", "index.html#autotoc_md37", null ],
      [ "HR 594 — программная перезагрузка", "index.html#autotoc_md38", null ],
      [ "HR 800 — сервисный PIN-код", "index.html#autotoc_md39", null ],
      [ "HR 900 — переход в bootloader", "index.html#autotoc_md40", null ]
    ] ],
    [ "Поведение при потере связи", "index.html#autotoc_md42", null ],
    [ "Сборка прошивки", "index.html#autotoc_md44", [
      [ "Рекомендуемые параметры проекта", "index.html#autotoc_md45", null ],
      [ "Порядок сборки", "index.html#autotoc_md46", null ]
    ] ],
    [ "Генерация Doxygen-документации", "index.html#autotoc_md48", null ],
    [ "Прошивка микроконтроллера", "index.html#autotoc_md50", [
      [ "Прошивка через ISP", "index.html#autotoc_md51", null ],
      [ "Прошивка через bootloader", "index.html#autotoc_md52", null ]
    ] ],
    [ "Проверка после прошивки", "index.html#autotoc_md54", null ],
    [ "Минимальный тест Modbus", "index.html#autotoc_md56", null ],
    [ "Возможные ошибки", "index.html#autotoc_md58", null ],
    [ "Примечания по сопровождению", "index.html#autotoc_md60", null ],
    [ "Состав передаваемых материалов", "index.html#autotoc_md62", null ],
    [ "Разделы", "topics.html", "topics" ],
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
"namespaceanonymous__namespace_02ads1115_8cpp_03.html#af0198035f5e2ba730ab031086678743f"
];

var SYNCONMSG = 'нажмите на выключить для синхронизации панелей';
var SYNCOFFMSG = 'нажмите на включить для синхронизации панелей';
var LISTOFALLMEMBERS = 'Полный список членов класса';