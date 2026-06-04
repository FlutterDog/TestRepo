
/**
 * @file usb_serial.cpp
 * @brief Реализация платформенного USB CDC-порта.
 */

#include "usb_serial.hpp"
#include "../hal/sam3x_usb_device.hpp"
#include "platform.hpp"

void UsbSerialPort::begin(uint32_t baudrate)
{
    (void)baudrate;
    hal_usb_cdc_begin();
}

void UsbSerialPort::begin(uint32_t baudrate, uint32_t frame)
{
    (void)baudrate;
    (void)frame;
    hal_usb_cdc_begin();
}

void UsbSerialPort::end(void)
{
    hal_usb_cdc_end();
}

int UsbSerialPort::available(void)
{
    return hal_usb_cdc_available();
}

int UsbSerialPort::availableForWrite(void)
{
    return hal_usb_cdc_available_for_write();
}

int UsbSerialPort::read(void)
{
    return hal_usb_cdc_read();
}

int UsbSerialPort::peek(void)
{
    return hal_usb_cdc_peek();
}

void UsbSerialPort::flush(void)
{
    hal_usb_cdc_flush();
}

size_t UsbSerialPort::write(uint8_t value)
{
    return write(&value, 1U);
}

size_t UsbSerialPort::write(const uint8_t* buffer, size_t size)
{
    return hal_usb_cdc_write(buffer, size);
}

uint32_t UsbSerialPort::baud(void)
{
    return hal_usb_cdc_baud();
}

uint8_t UsbSerialPort::stopbits(void)
{
    return hal_usb_cdc_stopbits();
}

uint8_t UsbSerialPort::paritytype(void)
{
    return hal_usb_cdc_paritytype();
}

uint8_t UsbSerialPort::numbits(void)
{
    return hal_usb_cdc_numbits();
}

bool UsbSerialPort::dtr(void)
{
    return hal_usb_cdc_dtr() != 0U;
}

bool UsbSerialPort::rts(void)
{
    return hal_usb_cdc_rts() != 0U;
}

bool UsbSerialPort::error(void)
{
    return hal_usb_cdc_error() != 0U;
}

void UsbSerialPort::clearError(void)
{
    hal_usb_cdc_clear_error();
}

UsbSerialPort::operator bool() const
{
    if (millis() < 500U)
    {
        return false;
    }

    return hal_usb_cdc_opened() != 0U;
}
