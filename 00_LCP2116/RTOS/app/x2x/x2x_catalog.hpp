/**
 * @file x2x_catalog.hpp
 * @brief Централизованный каталог поддерживаемых типов модулей X2X.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "x2x_module.hpp"
#include "x2x_types.hpp"

/** Тип функции создания объекта модуля в предоставленной памяти. */
typedef X2XDeviceHeader* (*X2XModuleConstructFunction)(void* storage,
                                                       size_t storage_size,
                                                       uint8_t slave_address,
                                                       uint16_t asdu);

/**
 * @brief Полное описание поддерживаемого типа X2X.
 *
 * Каталог является единой точкой регистрации типа: валидатор конфигурации,
 * реестр, диспетчер опроса и диагностика получают свойства модуля из одной
 * записи. При добавлении типа не требуется менять switch в X2X service или
 * service console.
 */
struct X2XModuleDescriptor
{
    X2XDeviceType type;
    const char* name;
    size_t object_size;
    uint16_t sp_count;
    uint16_t me_count;
    uint16_t tf_count;
    uint8_t allowed_in_x2x_config;
    X2XModuleConstructFunction construct;
    X2XModulePollFunction poll;
    X2XModulePrintFunction print;
};

/** @brief Возвращает описание типа либо null, если ID неизвестен. */
const X2XModuleDescriptor* x2x_catalog_find(X2XDeviceType type);

/** @brief Возвращает описание по числовому ID из конфигурационного файла. */
const X2XModuleDescriptor* x2x_catalog_find_by_id(uint16_t type_id);

/** @brief Возвращает количество зарегистрированных типов, включая LCP. */
size_t x2x_catalog_size(void);

/** @brief Возвращает запись каталога по порядковому индексу. */
const X2XModuleDescriptor* x2x_catalog_at(size_t index);
