# LCP Basic. Стартовый комплект платформенного слоя

## Назначение

Комплект добавляет минимальную основу прикладного проекта LCP Basic под ATSAM3X8E:

- точку входа `main()`;
- функции жизненного цикла `setup()` и `loop()`;
- верхний платформенный интерфейс;
- системный таймер на базе SysTick;
- функции времени `millis()`, `micros()`, `delay()` и `delayMicroseconds()`.

## Состав

```text
main.cpp
app/app.hpp
app/app.cpp
platform/platform.hpp
platform/binary_constants.hpp
platform/platform_time.cpp
hal/sam3x_device.hpp
hal/sam3x_tick.hpp
hal/sam3x_tick.cpp
```

## Подключение к проекту

Создать папки:

```text
app
platform
hal
```

Добавить в проект исходные файлы:

```text
app/app.cpp
platform/platform_time.cpp
hal/sam3x_tick.cpp
```

Заголовочные файлы добавить в проект для просмотра и сопровождения.

## Следующий этап

Следующий этап добавляет дискретный ввод-вывод:

- карту линий платы;
- HAL GPIO;
- `pinMode()`;
- `digitalWrite()`;
- `digitalRead()`.
