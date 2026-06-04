
/**
 * @file usb_serial.hpp
 * @brief Платформенный USB CDC-порт.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "print.hpp"

/**
 * @brief USB CDC-порт приложения.
 */
class UsbSerialPort : public Print
{
public:
    void begin(uint32_t baudrate);
    void begin(uint32_t baudrate, uint32_t frame);
    void end(void);

    int available(void);
    int availableForWrite(void);
    int read(void);
    int peek(void);
    void flush(void);

    size_t write(uint8_t value) override;
    size_t write(const uint8_t* buffer, size_t size);
    using Print::write;

    uint32_t baud(void);
    uint8_t stopbits(void);
    uint8_t paritytype(void);
    uint8_t numbits(void);
    bool dtr(void);
    bool rts(void);

    bool error(void);
    void clearError(void);

    operator bool() const;
};
