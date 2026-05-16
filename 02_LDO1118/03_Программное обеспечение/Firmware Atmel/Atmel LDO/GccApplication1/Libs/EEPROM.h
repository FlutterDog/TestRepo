#pragma once
#ifndef IGAS_EEPROM_SHIM_H
#define IGAS_EEPROM_SHIM_H

/**
 * @file EEPROM.h
 * @brief Тонкая C++-обёртка над EEPROM AVR для совместимости с историческим кодом.
 *
 * @details
 * Заголовок предоставляет минимальный интерфейс, близкий по форме к Arduino EEPROM API:
 * - `read()/write()/update()` для байтов;
 * - шаблонные `get()/put()` для чтения/записи структур блоком.
 *
 * Реальная реализация использует функции avr-libc из `<avr/eeprom.h>`.
 *
 * @note
 * Заголовок намеренно не включает другие проектные файлы (например, ADC_init.h, IGAS_Eeprom.h),
 * чтобы избежать циклических зависимостей и неконтролируемого расширения графа включений.
 *
 * @warning
 * Адрес EEPROM задаётся как `int` и интерпретируется как абсолютный байтовый адрес.
 * Корректность диапазона адресов должна обеспечиваться вызывающим кодом.
 */

#include <avr/eeprom.h>
#include <stdint.h>

/**
 * @class EEPROMClass
 * @brief Минимальный API для доступа к EEPROM.
 *
 * @details
 * Методы `write()` и `update()` отличаются тем, что `update()` выполняет физическую запись
 * только если значение изменилось, снижая износ EEPROM.
 */
class EEPROMClass {
public:
    /**
     * @brief Прочитать байт из EEPROM.
     * @param addr Абсолютный байтовый адрес в EEPROM.
     * @return Прочитанное значение.
     */
    uint8_t read(int addr) { return eeprom_read_byte((const uint8_t*)addr); }

    /**
     * @brief Записать байт в EEPROM (без проверки изменения).
     * @param addr Абсолютный байтовый адрес в EEPROM.
     * @param v Значение для записи.
     */
    void write(int addr, uint8_t v) { eeprom_write_byte((uint8_t*)addr, v); }

    /**
     * @brief Записать байт в EEPROM только при изменении значения.
     * @param addr Абсолютный байтовый адрес в EEPROM.
     * @param v Значение для записи.
     */
    void update(int addr, uint8_t v) { eeprom_update_byte((uint8_t*)addr, v); }

    /**
     * @brief Прочитать объект типа T из EEPROM (блочно).
     * @tparam T Тип читаемого объекта (POD/TriviallyCopyable предпочтительно).
     * @param addr Базовый адрес в EEPROM.
     * @param[out] out Буфер для результата.
     *
     * @note Для структур следует учитывать выравнивание и упаковку, если формат EEPROM
     * фиксирован и должен быть стабильным между версиями прошивки.
     */
    template <typename T>
    void get(int addr, T& out)
    {
        eeprom_read_block((void*)&out, (const void*)addr, sizeof(T));
    }

    /**
     * @brief Записать объект типа T в EEPROM (блочно) с update-семантикой.
     * @tparam T Тип записываемого объекта (POD/TriviallyCopyable предпочтительно).
     * @param addr Базовый адрес в EEPROM.
     * @param in Значение для записи.
     */
    template <typename T>
    void put(int addr, const T& in)
    {
        eeprom_update_block((const void*)&in, (void*)addr, sizeof(T));
    }
};

/**
 * @brief Глобальный экземпляр EEPROM.
 * @note Определение объекта должно находиться ровно в одном .cpp-файле.
 */
extern EEPROMClass EEPROM;

#endif // IGAS_EEPROM_SHIM_H
