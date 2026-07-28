
/**
 * @file serial_port.cpp
 * @brief Реализация платформенных аппаратных последовательных портов.
 */

#include "serial_port.hpp"
#include "platform.hpp"

SerialPort::SerialPort(HalUartId port_id)
    : port_id_(port_id),
      initialized_(0U)
{
}

void SerialPort::begin(uint32_t baudrate)
{
    begin(baudrate, SERIAL_8N1);
}

void SerialPort::begin(uint32_t baudrate, uint32_t frame)
{
    hal_uart_begin(port_id_, baudrate, convert_frame(frame));
    initialized_ = 1U;
}

void SerialPort::end(void)
{
    hal_uart_end(port_id_);
    initialized_ = 0U;
}

int SerialPort::available(void)
{
    return static_cast<int>(hal_uart_available(port_id_));
}

int SerialPort::read(void)
{
    return hal_uart_read(port_id_);
}

int SerialPort::peek(void)
{
    return hal_uart_peek(port_id_);
}

void SerialPort::flush(void)
{
    hal_uart_flush(port_id_);
}

size_t SerialPort::write(uint8_t value)
{
    if (initialized_ == 0U)
    {
        return 0U;
    }

    return hal_uart_write(port_id_, value);
}

size_t SerialPort::write(const uint8_t* buffer, size_t size)
{
    if ((initialized_ == 0U) || (buffer == 0))
    {
        return 0U;
    }

    size_t written = 0U;

    for (size_t index = 0U; index < size; ++index)
    {
        if (hal_uart_write(port_id_, buffer[index]) == 0U)
        {
            break;
        }

        ++written;
    }

    return written;
}

uint32_t SerialPort::errorCount(void) const
{
    return hal_uart_error_count(port_id_);
}

SerialPort::operator bool() const
{
    return (initialized_ != 0U);
}

HalUartFrame SerialPort::convert_frame(uint32_t frame) const
{
    if (frame == SERIAL_8O1)
    {
        return HAL_UART_FRAME_8O1;
    }

    if (frame == SERIAL_8E1)
    {
        return HAL_UART_FRAME_8E1;
    }

    return HAL_UART_FRAME_8N1;
}
