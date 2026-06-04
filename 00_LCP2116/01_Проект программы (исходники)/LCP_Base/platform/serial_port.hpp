
/**
 * @file serial_port.hpp
 * @brief Платформенный интерфейс аппаратного последовательного порта.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "print.hpp"
#include "../hal/sam3x_uart.hpp"

/**
 * @brief Аппаратный последовательный порт.
 */
class SerialPort : public Print
{
public:
    explicit SerialPort(HalUartId port_id);

    void begin(uint32_t baudrate);
    void begin(uint32_t baudrate, uint32_t frame);
    void end(void);

    int available(void);
    int read(void);
    void flush(void);

    size_t write(uint8_t value) override;
    using Print::write;

    operator bool() const;

private:
    HalUartFrame convert_frame(uint32_t frame) const;

    HalUartId port_id_;
    uint8_t initialized_;
};
