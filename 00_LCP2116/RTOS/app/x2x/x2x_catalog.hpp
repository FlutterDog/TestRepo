/**
 * @file x2x_catalog.hpp
 * @brief Централизованный каталог поддерживаемых типов модулей X2X.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "x2x_module.hpp"
#include "x2x_types.hpp"

/**
 * @brief Callback placement-конструирования объекта модуля в статическом слоте.
 *
 * @param[out] storage Начало выделенного слота реестра.
 * @param[in] storage_size Размер слота в байтах.
 * @param[in] slave_address Физический адрес X2X, совпадающий с позицией файла.
 * @param[in] asdu Логический ASDU устройства.
 * @return Заголовок созданного объекта либо null при ошибке.
 */
typedef X2XDeviceHeader* (*X2XModuleConstructFunction)(void* storage,
                                                       size_t storage_size,
                                                       uint8_t slave_address,
                                                       uint16_t asdu);

/**
 * @brief Полное описание одного поддерживаемого типа X2X.
 *
 * Каталог является единой точкой регистрации: валидатор конфигурации, реестр,
 * диспетчер опроса и диагностика используют одну запись. Добавление типа не
 * требует нового `switch` в X2X service или service console.
 */
struct X2XModuleDescriptor
{
    X2XDeviceType type;                   /**< Стабильный числовой ID X2X.TXT. */
    const char* name;                     /**< Краткое неизменяемое имя типа. */
    size_t object_size;                   /**< sizeof конкретной runtime-структуры. */
    uint16_t sp_count;                    /**< Число single-point значений. */
    uint16_t me_count;                    /**< Число integer measured values. */
    uint16_t tf_count;                    /**< Число float measured values. */
    uint8_t allowed_in_x2x_config;        /**< Разрешён ли ID во внешней цепочке. */
    X2XModuleConstructFunction construct; /**< Конструктор экземпляра в слоте. */
    X2XModulePollFunction poll;           /**< Неблокирующий драйвер типа. */
    X2XModulePrintFunction print;         /**< Специфическая диагностика типа. */
};

/**
 * @brief Ищет описание по типизированному ID.
 * @param[in] type Тип X2X.
 * @return Указатель на неизменяемую запись либо null.
 */
const X2XModuleDescriptor* x2x_catalog_find(X2XDeviceType type);

/**
 * @brief Ищет описание по числовому ID из X2X.TXT.
 * @param[in] type_id Значение из конфигурационного файла.
 * @return Указатель на неизменяемую запись либо null.
 */
const X2XModuleDescriptor* x2x_catalog_find_by_id(uint16_t type_id);

/** @return Количество зарегистрированных типов, включая центральный LCP. */
size_t x2x_catalog_size(void);

/**
 * @brief Возвращает запись каталога по порядковому индексу.
 * @param[in] index Индекс 0..x2x_catalog_size()-1.
 * @return Запись либо null для индекса вне диапазона.
 */
const X2XModuleDescriptor* x2x_catalog_at(size_t index);
