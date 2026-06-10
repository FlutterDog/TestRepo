
/**
 * @file lcp_usb_identity.hpp
 * @brief Сервисная USB-идентификация приложения LCP.
 *
 * Идентификаторы используются только для application firmware.
 * ROM-загрузчик SAM-BA использует собственную USB-идентификацию и после
 * перехода по 1200 baud определяется Windows как BOSSA/SAM-BA COM-порт.
 */

#pragma once

/**
 * @brief Тестовый VID для сервисного CDC-порта приложения.
 *
 * Значение не является зарегистрированным VID изделия.
 * Для серийной версии владелец продукта должен заменить VID/PID
 * на собственные или официально выделенные.
 */
#define LCP_USB_SERVICE_VID 0xCAFE

/**
 * @brief Тестовый PID для сервисного CDC-порта приложения.
 */
#define LCP_USB_SERVICE_PID 0x4011

/**
 * @brief Строка производителя в USB-дескрипторе приложения.
 */
#define LCP_USB_MANUFACTURER_STRING "LCP Service"

/**
 * @brief Строка продукта в USB-дескрипторе приложения.
 */
#define LCP_USB_PRODUCT_STRING "LCP Service CDC"
