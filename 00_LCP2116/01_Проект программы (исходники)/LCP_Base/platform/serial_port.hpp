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

/**
 * @brief Служебный USB-порт без доступа к USB-контроллеру.
 *
 * Класс сохраняет интерфейс последовательного порта и не выполняет операций
 * с USB-периферией. Все операции неблокирующие.
 */
class UsbSerialPort : public Print
{
public:
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
    uint8_t enabled_ = 0U;
};
