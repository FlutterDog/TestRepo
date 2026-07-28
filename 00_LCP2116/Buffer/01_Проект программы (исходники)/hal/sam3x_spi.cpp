/**
 * @file sam3x_spi.cpp
 * @brief Реализация HAL SPI для ATSAM3X8E.
 */

#include "sam3x_spi.hpp"
#include "sam3x_device.hpp"

static const uint32_t HAL_SPI_DEFAULT_CLOCK_HZ = 4000000U;
static const uint32_t HAL_SPI_PCS_VALUE = 0x0EU;

static void hal_enable_peripheral_clock(uint32_t peripheral_id)
{
    if (peripheral_id < 32U)
    {
        PMC->PMC_PCER0 = (1UL << peripheral_id);
    }
    else
    {
        PMC->PMC_PCER1 = (1UL << (peripheral_id - 32U));
    }
}

static void hal_spi_configure_pin(Pio* port, uint32_t mask, uint32_t pio_id, uint8_t peripheral_b)
{
    hal_enable_peripheral_clock(pio_id);

    port->PIO_IDR = mask;
    port->PIO_PUDR = mask;

    if (peripheral_b != 0U)
    {
        port->PIO_ABSR |= mask;
    }
    else
    {
        port->PIO_ABSR &= ~mask;
    }

    port->PIO_PDR = mask;
}

static uint32_t hal_spi_get_divisor(uint32_t clock_hz)
{
    if (clock_hz == 0U)
    {
        clock_hz = HAL_SPI_DEFAULT_CLOCK_HZ;
    }

    uint32_t divisor = (SystemCoreClock + clock_hz - 1U) / clock_hz;

    if (divisor == 0U)
    {
        divisor = 1U;
    }

    if (divisor > 255U)
    {
        divisor = 255U;
    }

    return divisor;
}

static uint32_t hal_spi_get_mode_bits(HalSpiMode mode)
{
    switch (mode)
    {
        case HAL_SPI_MODE_1:
            return 0U;

        case HAL_SPI_MODE_2:
            return SPI_CSR_CPOL | SPI_CSR_NCPHA;

        case HAL_SPI_MODE_3:
            return SPI_CSR_CPOL;

        case HAL_SPI_MODE_0:
        default:
            return SPI_CSR_NCPHA;
    }
}

static void hal_spi_flush_rx(void)
{
    while ((SPI0->SPI_SR & SPI_SR_RDRF) != 0U)
    {
        volatile uint32_t value = SPI0->SPI_RDR;
        (void)value;
    }
}

void hal_spi_begin(void)
{
    SystemCoreClockUpdate();

    hal_enable_peripheral_clock(ID_SPI0);

    hal_spi_configure_pin(PIOA, (1UL << 25), ID_PIOA, 0U);
    hal_spi_configure_pin(PIOA, (1UL << 26), ID_PIOA, 0U);
    hal_spi_configure_pin(PIOA, (1UL << 27), ID_PIOA, 0U);

    SPI0->SPI_CR = SPI_CR_SWRST;
    SPI0->SPI_CR = SPI_CR_SWRST;

    SPI0->SPI_MR = SPI_MR_MSTR
                 | SPI_MR_MODFDIS
                 | SPI_MR_PCS(HAL_SPI_PCS_VALUE);

    HalSpiConfig default_config;
    default_config.clock_hz = HAL_SPI_DEFAULT_CLOCK_HZ;
    default_config.bit_order = HAL_SPI_MSB_FIRST;
    default_config.mode = HAL_SPI_MODE_0;

    hal_spi_configure(default_config);

    hal_spi_flush_rx();

    SPI0->SPI_CR = SPI_CR_SPIEN;
}

void hal_spi_end(void)
{
    SPI0->SPI_CR = SPI_CR_SPIDIS;
}

void hal_spi_configure(const HalSpiConfig& config)
{
    const uint32_t divisor = hal_spi_get_divisor(config.clock_hz);

    const uint32_t csr = SPI_CSR_BITS_8_BIT
                       | SPI_CSR_SCBR(divisor)
                       | hal_spi_get_mode_bits(config.mode);

    SPI0->SPI_CSR[0] = csr;
}

uint8_t hal_spi_transfer(uint8_t value)
{
    while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0U)
    {
        __asm__ volatile ("nop");
    }

    SPI0->SPI_TDR = (static_cast<uint32_t>(value) & SPI_TDR_TD_Msk)
                  | SPI_TDR_PCS(HAL_SPI_PCS_VALUE);

    while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0U)
    {
        __asm__ volatile ("nop");
    }

    return static_cast<uint8_t>(SPI0->SPI_RDR & SPI_RDR_RD_Msk);
}
