/**
 * @file eeprom_map.h
 * @brief Карта EEPROM: адреса параметров и макросы доступа.
 *
 * @details
 * Адреса заданы в байтах. Для 16-битных параметров используются операции *word*,
 * для 8-битных — операции *byte*. Макросы чтения/записи являются тонкой обёрткой
 * над функциями AVR-LibC и выполняют запись в режиме *update* (запись только при
 * изменении значения), чтобы снизить износ EEPROM.
 *
 * @note
 * Этот заголовок определяет только «карту» и примитивы доступа. Политика версионирования
 * раскладки, проверка CRC и миграция (если требуется) должны выполняться на уровне
 * прикладной логики.
 */

#pragma once

#include <stdint.h>
#include <avr/io.h>
#include <avr/eeprom.h>

/**
 * @def EER(addr)
 * @brief Прочитать 16-битное слово из EEPROM.
 * @param addr Байтовый адрес (uint16_t), выровненность по 2 байтам — ответственность вызывающего.
 * @return Значение (uint16_t).
 */
#define EER(addr)        eeprom_read_word((const uint16_t*)(addr))

/**
 * @def EEW(addr, val)
 * @brief Записать (update) 16-битное слово в EEPROM.
 * @param addr Байтовый адрес (uint16_t).
 * @param val  Записываемое значение.
 */
#define EEW(addr, val)   eeprom_update_word((uint16_t*)(addr), (uint16_t)(val))

/**
 * @def EERB(addr)
 * @brief Прочитать байт из EEPROM.
 * @param addr Байтовый адрес (uint16_t).
 * @return Значение (uint8_t).
 */
#define EERB(addr)       eeprom_read_byte((const uint8_t*)(addr))

/**
 * @def EEWB(addr, val)
 * @brief Записать (update) байт в EEPROM.
 * @param addr Байтовый адрес (uint16_t).
 * @param val  Записываемое значение.
 */
#define EEWB(addr, val)  eeprom_update_byte((uint8_t*)(addr), (uint8_t)(val))

// =============================================================================
// Контроль раскладки EEPROM
// =============================================================================

/**
 * @def EEPROM_LAYOUT_VER
 * @brief Версия раскладки EEPROM (для контроля совместимости/миграций).
 */
#define EEPROM_LAYOUT_VER   1U

/**
 * @def EEPROM_BASE_ADDR
 * @brief Базовый адрес паспортного блока (если используется).
 */
#define EEPROM_BASE_ADDR    500U

/**
 * @def EEPROM_LAST_ADDR
 * @brief Последний используемый байтовый адрес EEPROM в данной раскладке.
 * @details Используется только для compile-time проверки размера EEPROM.
 */
#define EEPROM_LAST_ADDR    550U

/**
 * @def EEPROM_SIZE
 * @brief Размер EEPROM (байт) для целевого микроконтроллера.
 * @details Используется для compile-time проверки, что карта адресов не выходит за пределы.
 */
#if defined(__AVR_ATmega328P__)
    #define EEPROM_SIZE 1024U
#elif defined(__AVR_ATmega128__)
    #define EEPROM_SIZE 4096U
#else
    #define EEPROM_SIZE 1024U
#endif

#if (EEPROM_LAST_ADDR >= EEPROM_SIZE)
    #error "EEPROM map exceeds device EEPROM size"
#endif

// =============================================================================
// Рабочие/калибровочные параметры (используются в текущей прошивке)
// =============================================================================
//
// Примечания по типам:
//  - u8  = uint8_t   (один байт, читается/пишется через EERB/EEWB)
//  - u16 = uint16_t  (два байта, читается/пишется через EER/EEW)
//  - s16 = int16_t   (два байта, хранится как word; при чтении требуется приведение типов)
//
// Важно: значения адресов — часть внешнего контракта прошивки (сервис/обновления/совместимость).
//

/** @brief Порог «низкий сигнал сенсора» (u16). */
#define EE_LOW_SIGNAL       70U

/** @brief Серийный номер прибора (u16). */
#define EE_SERIAL_NO        72U

/** @brief Порог/оценка шума GAS-канала (u16). */
#define EE_GAS_NOISE        74U

/** @brief Индекс скорости RS-485 (u8; проектное перечисление). */
#define EE_BITRATE          76U

/** @brief Порог/оценка шума REF-канала (u16). */
#define EE_REF_NOISE        77U

/** @brief Термоградиент (u8; исторический формат). */
#define EE_THERMO_GRAD_B    79U

/** @brief Опция магнитной калибровки (u8; 0/1). */
#define EE_MAGNET_OPT       80U

/** @brief Усиление/режим АЦП (u8; проектное перечисление). */
#define EE_GAIN             81U

/** @brief Условный порог стабилизации PIR при старте (u8; проектные единицы). */
#define EE_STABLE_PIR_SP_B  82U

/** @brief Modbus Slave Address (u8; допустимо 1..247). */
#define EE_SLAVE_ADDR       87U

/** @brief Полный период измерительного цикла (u16, мс). */
#define EE_PERIOD_MS        90U

/** @brief Длительность окна свечения ИК-источника (u16, мс). */
#define EE_IRTIME_MS        92U

/** @brief Период семплирования (u8, мс; исторический формат). */
#define EE_SAMPLE_TIME_B    94U

/** @brief Разрешение нагрева кюветы (u8; 0/1). */
#define EE_HEATER_SWITCH    95U

/** @brief Порог «устойчивости» (u8; проектные единицы). */
#define EE_STABLE_GATE      96U

/** @brief Сдвиг нуля (u8). */
#define EE_ZERO_SHIFT       99U

/** @brief Маска блокировок/ошибок (u16). */
#define EE_BLOCK_ERROR      104U

/** @brief Порог «чистого окна» / clean-setpoint (u16; внутренние единицы). */
#define EE_CLEAN_SETPOINT   106U

/** @brief SpanZero (s16; может быть < 0). */
#define EE_SPAN_ZERO_S16    108U

/** @brief Год выпуска (u8; формат определяется проектом). */
#define EE_YEAR             111U

/** @brief Месяц выпуска (u8; 1..12). */
#define EE_MONTH            112U

/** @brief Нижний порог включения нагрева (u16). */
#define EE_LOWER_HEAT       116U

/** @brief Верхний порог выключения нагрева (u16). */
#define EE_UPPER_HEAT       118U

/** @brief Реверс аналогового входа/каналов (u8; 0/1). */
#define EE_REVERSE_AI       125U

/** @brief Положение потенциометра (u8; исторический формат). */
#define EE_POT_TARGET_B     126U

/** @brief Тип потенциометра (u8; проектное перечисление). */
#define EE_POT_TYPE         127U

/** @brief Нормирующий коэффициент PIR1 (u16; масштаб x1000). */
#define EE_NORM_PIR1_x1000  131U

/** @brief Нормирующий коэффициент REF (u16; масштаб x1000). */
#define EE_NORM_REF_x1000   133U

/** @brief Служебная/диагностическая ячейка (u16). */
#define EE_SCRATCH_300      300U

// =============================================================================
// Паспортный блок (структура данных верхнего уровня; используется по проекту)
// =============================================================================
//
// Если часть полей не используется текущей прошивкой, они всё равно входят в
// контракт раскладки EEPROM и должны сохранять адреса при обновлениях.
//

/** @brief Код целевого газа (u16; перечисление проекта). */
#define EE_GAS_CODE         500U

/** @brief Код единиц измерения (u16; перечисление проекта). */
#define EE_UNITS_CODE       502U

/** @brief Диапазон: минимум (s16) в единицах EE_UNITS_CODE. */
#define EE_RANGE_MIN        504U

/** @brief Диапазон: максимум/FullScale (s16) в единицах EE_UNITS_CODE. */
#define EE_RANGE_MAX        506U

/** @brief Tmin эксплуатации (s16, °C). */
#define EE_TMIN_C           508U

/** @brief Tmax эксплуатации (s16, °C). */
#define EE_TMAX_C           510U

/** @brief Номинал питания (u16; 120 = 12.0 В). */
#define EE_VNOM_01V         512U

/** @brief Гарантия (u16; месяцев). */
#define EE_WARRANTY_MON     514U

/** @brief Ревизия аппаратуры/PCB (u16). */
#define EE_HW_REV           516U

/** @brief Наработка: младшие 16 бит (u16; часы). */
#define EE_RUNTIME_LO       518U

/** @brief Наработка: старшие 16 бит (u16; часы). */
#define EE_RUNTIME_HI       520U

/** @brief Количество включений питания (u16). */
#define EE_POWER_CYCLES     522U

/** @brief Год последнего обслуживания/поверки (u16; YYYY). */
#define EE_SVC_YEAR         524U

/** @brief Месяц последнего обслуживания/поверки (u16; 1..12). */
#define EE_SVC_MONTH        526U

/** @brief Порог тревоги 1 (s16; в единицах измерения). */
#define EE_ALARM1           528U

/** @brief Порог тревоги 2 (s16; в единицах измерения). */
#define EE_ALARM2           530U

/** @brief Флаги опций (u16; битовая маска). */
#define EE_OPTION_FLAGS     532U

/** @brief Вариант исполнения/модификация (u16). */
#define EE_VARIANT_CODE     534U

/** @brief Последний код ошибки (u16). */
#define EE_LAST_ERROR       536U

/** @brief Счётчик ошибок (u16). */
#define EE_ERROR_COUNT      538U

/** @brief Год следующей поверки (u16; YYYY). */
#define EE_NEXTVER_YEAR     540U

/** @brief Месяц следующей поверки (u16; 1..12). */
#define EE_NEXTVER_MONTH    542U

/** @brief Тип сенсорного модуля (u16; перечисление). */
#define EE_SENSOR_TYPE      544U

/** @brief Код страны производства (u16; внутренний/ISO). */
#define EE_COUNTRY_CODE     546U

/** @brief CRC паспортного блока (u16; алгоритм задаётся проектом). */
#define EE_INFO_CRC         548U

/** @brief Резерв (u16). */
#define EE_RESERVED_69      550U
