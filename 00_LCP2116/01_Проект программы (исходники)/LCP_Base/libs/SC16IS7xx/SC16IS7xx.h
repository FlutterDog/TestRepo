/**
 * @file SC16IS7xx.h
 * @brief Драйвер внешних UART-контроллеров SC16IS7xx по SPI.
 *
 * Драйвер предоставляет операции инициализации UART-канала, чтения,
 * записи, проверки приёма и прямого доступа к регистрам SC16IS7xx.
 */

#pragma once

#include <stdint.h>

#include "../../platform/platform.hpp"

static const uint8_t CH_A = 0x00U;
static const uint8_t CH_B = 0x01U;
static const uint8_t SC16IS752_CHANNEL_BOTH = 0x00U;

static const uint32_t BAUD_RATE_DEFAULT = 9600UL;

/**
 * @brief SPI-драйвер внешнего UART-контроллера SC16IS7xx.
 *
 * Методы принимают линию Chip Select и номер канала. Это позволяет одному
 * объекту обслуживать несколько микросхем SC16IS7xx на общей SPI-шине.
 */
class SpiUartDevice
{
public:
    void begin(unsigned long baudrate = BAUD_RATE_DEFAULT,
               byte parityMode = 0,
               byte chip_select = 53,
               uint8_t channel = CH_A);

    int available(byte chip_select, uint8_t channel);
    int read(byte chip_select, uint8_t channel);
    void write(byte value, byte chip_select, uint8_t channel);
    void flush(byte chip_select, uint8_t channel);

    void ioSetDirection(unsigned char bits, byte chip_select, uint8_t channel);
    void ioSetState(unsigned char bits, byte chip_select, uint8_t channel);

    void deselect(byte chip_select);
    void select(byte chip_select);

    void writeRegister(byte registerAddress, byte data, byte chip_select, uint8_t channel);
    byte readRegister(byte registerAddress, byte chip_select, uint8_t channel);

    void initUart(unsigned long baudrate, byte parityMode, byte chip_select, uint8_t channel);
    void configureUart(unsigned long baudrate, byte parityMode, byte chip_select, uint8_t channel);
    void setBaudRate(unsigned long baudrate, byte chip_select, uint8_t channel);

    boolean uartConnected(byte chip_select, uint8_t channel);
};
