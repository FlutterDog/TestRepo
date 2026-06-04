
/**
 * @file sam3x_usb_device.hpp
 * @brief USB CDC device для ATSAM3X8E.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Инициализирует USB CDC и подключает устройство к USB-шине.
 */
void hal_usb_cdc_begin(void);

/**
 * @brief Отключает USB CDC от USB-шины.
 */
void hal_usb_cdc_end(void);

/**
 * @brief Возвращает признак активной USB-конфигурации.
 */
uint8_t hal_usb_cdc_configured(void);

/**
 * @brief Возвращает признак открытия CDC-порта со стороны host.
 */
uint8_t hal_usb_cdc_opened(void);

/**
 * @brief Возвращает количество байтов в RX-буфере CDC.
 */
int hal_usb_cdc_available(void);

/**
 * @brief Возвращает количество байтов, доступных для записи в CDC IN endpoint.
 */
int hal_usb_cdc_available_for_write(void);

/**
 * @brief Читает один байт из CDC RX-буфера.
 */
int hal_usb_cdc_read(void);

/**
 * @brief Возвращает следующий байт CDC RX-буфера без удаления.
 */
int hal_usb_cdc_peek(void);

/**
 * @brief Передаёт буфер через CDC IN endpoint.
 */
size_t hal_usb_cdc_write(const uint8_t* buffer, size_t size);

/**
 * @brief Сбрасывает накопленный пакет CDC IN endpoint.
 */
void hal_usb_cdc_flush(void);

/**
 * @brief Возвращает скорость, установленную host через CDC SET_LINE_CODING.
 */
uint32_t hal_usb_cdc_baud(void);

/**
 * @brief Возвращает количество стоп-битов из CDC SET_LINE_CODING.
 */
uint8_t hal_usb_cdc_stopbits(void);

/**
 * @brief Возвращает тип паритета из CDC SET_LINE_CODING.
 */
uint8_t hal_usb_cdc_paritytype(void);

/**
 * @brief Возвращает количество битов данных из CDC SET_LINE_CODING.
 */
uint8_t hal_usb_cdc_numbits(void);

/**
 * @brief Возвращает состояние DTR.
 */
uint8_t hal_usb_cdc_dtr(void);

/**
 * @brief Возвращает состояние RTS.
 */
uint8_t hal_usb_cdc_rts(void);

/**
 * @brief Возвращает признак ошибки USB-обмена.
 */
uint8_t hal_usb_cdc_error(void);

/**
 * @brief Сбрасывает флаги ошибок USB-обмена.
 */
void hal_usb_cdc_clear_error(void);
