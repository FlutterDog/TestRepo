# Добавление нового модуля X2X

## Назначение архитектуры

X2X разделён на несколько уровней, но новый модуль не требует изменения каждого из них.

Служебные уровни работают одинаково для всех типов:

- `x2x_config` читает и проверяет `X2X.TXT`;
- `x2x_registry` создаёт статические экземпляры модулей;
- `x2x_service` циклически вызывает драйвер текущего slave;
- `modbus_rtu_master` выполняет неблокирующие транзакции;
- `x2x_status` печатает общий статус связи.

Специфика конкретного модуля сосредоточена в трёх местах:

1. структура данных и стабильный ID;
2. функция опроса;
3. одна запись в каталоге.

Новый `.cpp` также необходимо добавить в проект Microchip Studio.

## Шаг 1. Назначить стабильный ID и описать данные

Файл:

```text
app/x2x/x2x_types.hpp
```

Добавить новый ID в `X2XDeviceType`.

Пример:

```cpp
X2X_DEVICE_LTM1114 = 7U
```

После выпуска прошивки числовой ID нельзя изменять или использовать повторно, потому что он сохраняется в `X2X.TXT`.

Добавить структуру runtime-данных:

```cpp
struct X2XLtm1114 : X2XDeviceHeader
{
    static const uint8_t SP_COUNT = 1U;
    static const uint8_t ME_COUNT = 0U;
    static const uint8_t TF_COUNT = 4U;

    uint32_t digital_inputs;
    float tf_values[TF_COUNT];
};
```

Структура содержит последнее корректно принятое значение. При потере связи значения не обнуляются. Их актуальность определяется полями общего заголовка:

```text
connection_lost
device_fault
last_communication_error
last_update_ms
```

## Шаг 2. Реализовать неблокирующий драйвер

Создать файл, например:

```text
app/x2x/modules/x2x_ltm.cpp
```

Объявить функцию в:

```text
app/x2x/modules/x2x_module_drivers.hpp
```

Сигнатура:

```cpp
X2XModulePollResult x2x_module_poll_ltm1114(
    X2XDeviceHeader* device,
    X2XModuleContext& context);
```

Драйвер не должен использовать блокирующие циклы ожидания. Один вызов выполняет только один шаг машины состояний:

```text
проверить готовность master
запустить запрос
вернуться IN_PROGRESS
дождаться результата при следующих вызовах
декодировать данные
зафиксировать success или failure
вернуть CYCLE_COMPLETE
```

Для успешного обмена используется:

```cpp
x2x_module_mark_success(*device, context.now_ms);
```

Для ошибки:

```cpp
x2x_module_mark_failure(
    *device,
    modbus_rtu_master_result(*context.master),
    modbus_rtu_master_exception_code(*context.master));
```

Для преобразования low-word-first пары регистров в `float` используются:

```cpp
x2x_module_registers_to_float(...)
x2x_module_decode_float_array(...)
```

## Шаг 3. Добавить одну запись в каталог

Файл:

```text
app/x2x/x2x_catalog.cpp
```

Пример:

```cpp
{
    X2X_DEVICE_LTM1114,
    "LTM1114",
    sizeof(X2XLtm1114),
    X2XLtm1114::SP_COUNT,
    X2XLtm1114::ME_COUNT,
    X2XLtm1114::TF_COUNT,
    1U,
    construct_device<X2XLtm1114, X2X_DEVICE_LTM1114>,
    x2x_module_poll_ltm1114,
    print_digital_inputs_and_floats<X2XLtm1114>
}
```

Эта одна запись автоматически подключает модуль к:

- проверке ID из `X2X.TXT`;
- статическому реестру;
- циклическому диспетчеру опроса;
- общему статусу связи;
- печати имени и количества SP/ME/TF;
- специфическому диагностическому выводу.

Для простого дискретного модуля можно использовать:

```cpp
print_digital_inputs<X2XLtm1114>
```

Для модуля с DI и массивом `tf_values`:

```cpp
print_digital_inputs_and_floats<X2XLtm1114>
```

Для нестандартного модуля создаётся отдельная функция печати внутри `x2x_catalog.cpp`.

В `x2x_status.cpp` добавлять новый `case` больше не требуется.

## Статический реестр

Каждый экземпляр занимает один статический слот:

```cpp
X2X_DEVICE_SLOT_BYTES = 256U
```

При регистрации типа шаблон `construct_device()` выполняет:

```cpp
static_assert(sizeof(DeviceType) <= X2X_DEVICE_SLOT_BYTES);
```

Если структура нового модуля слишком велика, проект не соберётся. Скрытого переполнения памяти не будет.

Размер реестра больше не зависит от `sizeof(X2XLct1114_2)` и не требует ручного изменения при добавлении обычного модуля.

## Шаг 4. Добавить исходник в проект

Добавить созданный `.cpp` в `LCP_Basic.cppproj` через Microchip Studio либо вручную:

```xml
<Compile Include="app\x2x\modules\x2x_ltm.cpp">
    <SubType>compile</SubType>
</Compile>
```

## Шаг 5. Добавить модуль в X2X.TXT

Пример для одного модуля с ID 7:

```text
1
7
Fin
```

Позиция строки задаёт Modbus slave address. В данном примере адрес равен 1.

## Обязательная проверка

После добавления типа проверить:

1. Debug и Release собираются без ошибок и предупреждений.
2. `x2x reload` принимает новый ID.
3. Команда `x2x` показывает правильное имя и SP/ME/TF.
4. Счётчик `success` растёт.
5. При отключении линии растут `failed` и `consecutive_failures`.
6. После пяти ошибок появляется `connection=lost`.
7. После восстановления линия автоматически возвращается в `connection=online`.
8. Последние корректные значения сохраняются, но считаются невалидными при ошибке связи.
9. `rtos` не показывает нежелательного роста heap или стека.

## Что не нужно менять

При обычном добавлении модуля не изменяются:

```text
x2x_config.cpp
x2x_registry.cpp
x2x_service.cpp
x2x_status.cpp
modbus_rtu_master.cpp
```

Изменения этих файлов нужны только при изменении общей архитектуры X2X, а не при добавлении очередной карты регистров.
