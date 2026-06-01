/**
 * @file serial_port.cpp
 * @brief Реализация платформенных последовательных портов.
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

    hal_uart_write(port_id_, value);
    return 1U;
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

void UsbSerialPort::begin(uint32_t baudrate)
{
    (void)baudrate;
    enabled_ = 1U;
}

void UsbSerialPort::begin(uint32_t baudrate, uint32_t frame)
{
    (void)baudrate;
    (void)frame;
    enabled_ = 1U;
}

void UsbSerialPort::end(void)
{
    enabled_ = 0U;
}

int UsbSerialPort::available(void)
{
    return 0;
}

int UsbSerialPort::read(void)
{
    return -1;
}

void UsbSerialPort::flush(void)
{
}

size_t UsbSerialPort::write(uint8_t value)
{
    (void)value;

    if (enabled_ == 0U)
    {
        return 0U;
    }

    return 1U;
}

UsbSerialPort::operator bool() const
{
    return (enabled_ != 0U);
}
