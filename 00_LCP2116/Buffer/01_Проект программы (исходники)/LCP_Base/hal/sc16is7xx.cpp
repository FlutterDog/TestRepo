/**
 * @file sc16is7xx.cpp
 * @brief Реализация HAL внешних UART SC16IS7xx по SPI.
 */

#include "sc16is7xx.hpp"
#include "../platform/platform.hpp"

static const uint32_t SC16IS_XTAL_HZ = 14745600UL;

static const uint8_t SC16IS_REG_RHR = 0x00U;
static const uint8_t SC16IS_REG_THR = 0x00U;
static const uint8_t SC16IS_REG_IER = 0x01U;
static const uint8_t SC16IS_REG_FCR = 0x02U;
static const uint8_t SC16IS_REG_LCR = 0x03U;
static const uint8_t SC16IS_REG_MCR = 0x04U;
static const uint8_t SC16IS_REG_LSR = 0x05U;
static const uint8_t SC16IS_REG_SPR = 0x07U;
static const uint8_t SC16IS_REG_TXLVL = 0x08U;
static const uint8_t SC16IS_REG_RXLVL = 0x09U;
static const uint8_t SC16IS_REG_EFCR = 0x0FU;
static const uint8_t SC16IS_REG_DLL = 0x00U;
static const uint8_t SC16IS_REG_DLH = 0x01U;

static const uint8_t SC16IS_LCR_8N1 = 0x03U;
static const uint8_t SC16IS_LCR_ENABLE_DIVISOR = 0x80U;

static const uint8_t SC16IS_FCR_ENABLE_FIFO = 0x01U;
static const uint8_t SC16IS_FCR_RX_FIFO_RESET = 0x02U;
static const uint8_t SC16IS_FCR_TX_FIFO_RESET = 0x04U;

static const uint8_t SC16IS_LSR_DATA_READY = 0x01U;
static const uint8_t SC16IS_LSR_THR_EMPTY = 0x20U;

static const uint8_t SC16IS_EFCR_RS485_ENABLE_RTS = 0x10U;
static const uint8_t SC16IS_EFCR_RTS_INVERT = 0x20U;
static const uint8_t SC16IS_EFCR_RS485_MODE = 0x01U;

static const uint8_t SC16IS_SPI_WRITE = 0x00U;
static const uint8_t SC16IS_SPI_READ = 0x80U;

static uint8_t sc16is_channel_bits(Sc16isChannel channel)
{
    return (channel == SC16IS_CHANNEL_B) ? 0x02U : 0x00U;
}

static uint8_t sc16is_address(uint8_t reg, Sc16isChannel channel, uint8_t read_flag)
{
    return static_cast<uint8_t>((reg << 3U) | sc16is_channel_bits(channel) | read_flag);
}

static uint8_t sc16is_transfer(uint8_t value)
{
    return SPI.transfer(value);
}

static void sc16is_select(uint32_t chip_select)
{
    digitalWrite(chip_select, LOW);
}

static void sc16is_deselect(uint32_t chip_select)
{
    digitalWrite(chip_select, HIGH);
}

static void sc16is_write_register(uint32_t chip_select, Sc16isChannel channel, uint8_t reg, uint8_t value)
{
    sc16is_select(chip_select);
    (void)sc16is_transfer(sc16is_address(reg, channel, SC16IS_SPI_WRITE));
    (void)sc16is_transfer(value);
    sc16is_deselect(chip_select);
}

static uint8_t sc16is_read_register(uint32_t chip_select, Sc16isChannel channel, uint8_t reg)
{
    sc16is_select(chip_select);
    (void)sc16is_transfer(sc16is_address(reg, channel, SC16IS_SPI_READ));
    const uint8_t value = sc16is_transfer(0xFFU);
    sc16is_deselect(chip_select);

    return value;
}

void sc16is_init_bus(void)
{
    SPI.begin();
}

Sc16isProbeResult sc16is_probe_channel(uint32_t chip_select, Sc16isChannel channel)
{
    static const uint8_t first = 0x55U;
    static const uint8_t second = 0xAAU;

    sc16is_write_register(chip_select, channel, SC16IS_REG_SPR, first);
    const uint8_t first_read = sc16is_read_register(chip_select, channel, SC16IS_REG_SPR);

    sc16is_write_register(chip_select, channel, SC16IS_REG_SPR, second);
    const uint8_t second_read = sc16is_read_register(chip_select, channel, SC16IS_REG_SPR);

    return ((first_read == first) && (second_read == second)) ? SC16IS_PROBE_PRESENT : SC16IS_PROBE_ABSENT;
}

uint8_t sc16is_probe_dual_channel(uint32_t chip_select)
{
    static const uint8_t value_a = 0x11U;
    static const uint8_t value_b = 0x22U;

    if (sc16is_probe_channel(chip_select, SC16IS_CHANNEL_A) != SC16IS_PROBE_PRESENT)
    {
        return 0U;
    }

    sc16is_write_register(chip_select, SC16IS_CHANNEL_A, SC16IS_REG_SPR, value_a);
    sc16is_write_register(chip_select, SC16IS_CHANNEL_B, SC16IS_REG_SPR, value_b);

    const uint8_t read_a = sc16is_read_register(chip_select, SC16IS_CHANNEL_A, SC16IS_REG_SPR);
    const uint8_t read_b = sc16is_read_register(chip_select, SC16IS_CHANNEL_B, SC16IS_REG_SPR);

    return ((read_a == value_a) && (read_b == value_b)) ? 1U : 0U;
}

void sc16is_begin(uint32_t chip_select, Sc16isChannel channel, uint32_t baudrate)
{
    if (baudrate == 0U)
    {
        return;
    }

    const uint32_t divisor = SC16IS_XTAL_HZ / (baudrate * 16UL);

    sc16is_write_register(chip_select, channel, SC16IS_REG_IER, 0x00U);
    sc16is_write_register(chip_select, channel, SC16IS_REG_LCR, SC16IS_LCR_ENABLE_DIVISOR);
    sc16is_write_register(chip_select, channel, SC16IS_REG_DLL, static_cast<uint8_t>(divisor & 0xFFU));
    sc16is_write_register(chip_select, channel, SC16IS_REG_DLH, static_cast<uint8_t>((divisor >> 8U) & 0xFFU));
    sc16is_write_register(chip_select, channel, SC16IS_REG_LCR, SC16IS_LCR_8N1);
    sc16is_write_register(chip_select, channel, SC16IS_REG_FCR,
                          SC16IS_FCR_ENABLE_FIFO | SC16IS_FCR_RX_FIFO_RESET | SC16IS_FCR_TX_FIFO_RESET);
    sc16is_write_register(chip_select, channel, SC16IS_REG_MCR, 0x00U);
    sc16is_write_register(chip_select, channel, SC16IS_REG_EFCR,
                          SC16IS_EFCR_RS485_ENABLE_RTS | SC16IS_EFCR_RTS_INVERT | SC16IS_EFCR_RS485_MODE);
}

uint8_t sc16is_available(uint32_t chip_select, Sc16isChannel channel)
{
    return sc16is_read_register(chip_select, channel, SC16IS_REG_RXLVL);
}

int sc16is_read(uint32_t chip_select, Sc16isChannel channel)
{
    if ((sc16is_read_register(chip_select, channel, SC16IS_REG_LSR) & SC16IS_LSR_DATA_READY) == 0U)
    {
        return -1;
    }

    return sc16is_read_register(chip_select, channel, SC16IS_REG_RHR);
}

uint8_t sc16is_tx_available(uint32_t chip_select, Sc16isChannel channel)
{
    return sc16is_read_register(chip_select, channel, SC16IS_REG_TXLVL);
}

size_t sc16is_write(uint32_t chip_select, Sc16isChannel channel, uint8_t value)
{
    if ((sc16is_read_register(chip_select, channel, SC16IS_REG_LSR) & SC16IS_LSR_THR_EMPTY) == 0U)
    {
        return 0U;
    }

    sc16is_write_register(chip_select, channel, SC16IS_REG_THR, value);
    return 1U;
}

size_t sc16is_write_buffer(uint32_t chip_select, Sc16isChannel channel, const uint8_t* buffer, size_t length)
{
    if (buffer == 0)
    {
        return 0U;
    }

    size_t written = 0U;

    while (written < length)
    {
        uint8_t available = sc16is_tx_available(chip_select, channel);

        if (available == 0U)
        {
            break;
        }

        while ((available > 0U) && (written < length))
        {
            sc16is_write_register(chip_select, channel, SC16IS_REG_THR, buffer[written]);
            ++written;
            --available;
        }
    }

    return written;
}

uint8_t sc16is_line_status(uint32_t chip_select, Sc16isChannel channel)
{
    return sc16is_read_register(chip_select, channel, SC16IS_REG_LSR);
}
