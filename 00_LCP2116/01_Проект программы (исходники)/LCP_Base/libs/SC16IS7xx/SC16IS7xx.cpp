/**
 * @file SC16IS7xx.cpp
 * @brief Реализация драйвера внешних UART-контроллеров SC16IS7xx.
 */

#include "SC16IS7xx.h"

static const byte SC16_REG_THR      = static_cast<byte>(0x00U << 3);
static const byte SC16_REG_RHR      = static_cast<byte>(0x00U << 3);
static const byte SC16_REG_FCR      = static_cast<byte>(0x02U << 3);
static const byte SC16_REG_LCR      = static_cast<byte>(0x03U << 3);
static const byte SC16_REG_SPR      = static_cast<byte>(0x07U << 3);
static const byte SC16_REG_TXLVL    = static_cast<byte>(0x08U << 3);
static const byte SC16_REG_RXLVL    = static_cast<byte>(0x09U << 3);
static const byte SC16_REG_IODIR    = static_cast<byte>(0x0AU << 3);
static const byte SC16_REG_IOSTATE  = static_cast<byte>(0x0BU << 3);
static const byte SC16_REG_EFCR     = static_cast<byte>(0x0FU << 3);

static const byte SC16_REG_DLL      = static_cast<byte>(0x00U << 3);
static const byte SC16_REG_DLM      = static_cast<byte>(0x01U << 3);

static const byte SC16_SPI_READ     = 0x80U;

static const byte EFCR_ENABLE_RTS   = static_cast<byte>(1U << 4);
static const byte EFCR_INVERSION    = static_cast<byte>(1U << 5);
static const byte EFCR_MULTIDROP    = 0x01U;

static const byte LCR_ENABLE_DIVISOR_LATCH = static_cast<byte>(1U << 7);
static const byte LCR_ENABLE_PARITY        = static_cast<byte>(1U << 3);
static const byte LCR_EVEN_PARITY          = static_cast<byte>(1U << 4);
static const byte LCR_8BIT_LENGTH          = 0x03U;

static const byte FCR_RESET_TX_FIFO = 0x04U;
static const byte FCR_RESET_RX_FIFO = 0x02U;
static const byte FCR_ENABLE_FIFO   = 0x01U;

static const uint32_t SC16_XTAL_FREQUENCY = 14745600UL;
static const uint32_t SC16_PRESCALER = 1UL;
static const uint32_t SC16_SPI_CLOCK_HZ = 4000000UL;

static const byte SC16_DATA_FORMAT_8N1 = 0x03U;
static const byte SC16_RS485_AUTOCONTROL = static_cast<byte>(EFCR_ENABLE_RTS | EFCR_INVERSION | EFCR_MULTIDROP);

static byte sc16_low_byte(uint32_t value)
{
    return static_cast<byte>(value & 0xFFU);
}

static byte sc16_high_byte(uint32_t value)
{
    return static_cast<byte>((value >> 8) & 0xFFU);
}

static uint32_t sc16_baud_divisor(uint32_t baudrate)
{
    if (baudrate == 0U)
    {
        baudrate = BAUD_RATE_DEFAULT;
    }

    return (SC16_XTAL_FREQUENCY / SC16_PRESCALER) / (baudrate * 16UL);
}

static byte sc16_address(byte registerAddress, uint8_t channel)
{
    return static_cast<byte>(registerAddress | static_cast<byte>(channel << 1));
}

void SpiUartDevice::begin(unsigned long baudrate, byte parityMode, byte chip_select, uint8_t channel)
{
    initUart(baudrate, parityMode, chip_select, channel);
}

void SpiUartDevice::deselect(byte chip_select)
{
    digitalWrite(chip_select, HIGH);
}

void SpiUartDevice::select(byte chip_select)
{
    digitalWrite(chip_select, LOW);
}

void SpiUartDevice::initUart(unsigned long baudrate, byte parityMode, byte chip_select, uint8_t channel)
{
    configureUart(baudrate, parityMode, chip_select, channel);
    (void)uartConnected(chip_select, channel);
}

void SpiUartDevice::setBaudRate(unsigned long baudrate, byte chip_select, uint8_t channel)
{
    const uint32_t divisor = sc16_baud_divisor(baudrate);

    writeRegister(SC16_REG_LCR, LCR_ENABLE_DIVISOR_LATCH, chip_select, channel);
    writeRegister(SC16_REG_DLL, sc16_low_byte(divisor), chip_select, channel);
    writeRegister(SC16_REG_DLM, sc16_high_byte(divisor), chip_select, channel);
}

void SpiUartDevice::configureUart(unsigned long baudrate, byte parityMode, byte chip_select, uint8_t channel)
{
    setBaudRate(baudrate, chip_select, channel);

    if (parityMode == 1U)
    {
        writeRegister(SC16_REG_LCR,
                      static_cast<byte>(LCR_ENABLE_PARITY | LCR_8BIT_LENGTH),
                      chip_select,
                      channel);
    }
    else if (parityMode == 2U)
    {
        writeRegister(SC16_REG_LCR,
                      static_cast<byte>(LCR_ENABLE_PARITY | LCR_EVEN_PARITY | LCR_8BIT_LENGTH),
                      chip_select,
                      channel);
    }
    else
    {
        writeRegister(SC16_REG_LCR, SC16_DATA_FORMAT_8N1, chip_select, channel);
    }

    writeRegister(SC16_REG_EFCR, SC16_RS485_AUTOCONTROL, chip_select, channel);

    writeRegister(SC16_REG_FCR,
                  static_cast<byte>(FCR_RESET_TX_FIFO | FCR_RESET_RX_FIFO),
                  chip_select,
                  channel);

    writeRegister(SC16_REG_FCR, FCR_ENABLE_FIFO, chip_select, channel);
}

boolean SpiUartDevice::uartConnected(byte chip_select, uint8_t channel)
{
    static const byte test_value = 0x55U;

    writeRegister(SC16_REG_SPR, test_value, chip_select, channel);

    return (readRegister(SC16_REG_SPR, chip_select, channel) == test_value);
}

void SpiUartDevice::writeRegister(byte registerAddress, byte data, byte chip_select, uint8_t channel)
{
    SPI.beginTransaction(SPISettings(SC16_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0));

    select(chip_select);
    SPI.transfer(sc16_address(registerAddress, channel));
    SPI.transfer(data);
    deselect(chip_select);

    SPI.endTransaction();
}

byte SpiUartDevice::readRegister(byte registerAddress, byte chip_select, uint8_t channel)
{
    static const byte dummy = 0xFFU;

    SPI.beginTransaction(SPISettings(SC16_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0));

    select(chip_select);
    SPI.transfer(static_cast<byte>(SC16_SPI_READ | sc16_address(registerAddress, channel)));
    const byte result = SPI.transfer(dummy);
    deselect(chip_select);

    SPI.endTransaction();

    return result;
}

int SpiUartDevice::available(byte chip_select, uint8_t channel)
{
    return static_cast<int>(readRegister(SC16_REG_RXLVL, chip_select, channel));
}

int SpiUartDevice::read(byte chip_select, uint8_t channel)
{
    if (available(chip_select, channel) == 0)
    {
        return -1;
    }

    return static_cast<int>(readRegister(SC16_REG_RHR, chip_select, channel));
}

void SpiUartDevice::write(byte value, byte chip_select, uint8_t channel)
{
    const uint32_t started = micros();

    while ((readRegister(SC16_REG_TXLVL, chip_select, channel) == 0U)
        && ((uint32_t)(micros() - started) < 200U))
    {
    }

    writeRegister(SC16_REG_THR, value, chip_select, channel);
}

void SpiUartDevice::flush(byte chip_select, uint8_t channel)
{
    while (available(chip_select, channel) > 0)
    {
        (void)read(chip_select, channel);
    }
}

void SpiUartDevice::ioSetDirection(unsigned char bits, byte chip_select, uint8_t channel)
{
    writeRegister(SC16_REG_IODIR, static_cast<byte>(bits), chip_select, channel);
}

void SpiUartDevice::ioSetState(unsigned char bits, byte chip_select, uint8_t channel)
{
    writeRegister(SC16_REG_IOSTATE, static_cast<byte>(bits), chip_select, channel);
}
