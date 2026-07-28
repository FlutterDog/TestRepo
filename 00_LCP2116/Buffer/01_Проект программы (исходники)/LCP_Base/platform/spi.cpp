/**
 * @file spi.cpp
 * @brief Реализация платформенного интерфейса SPI.
 */

#include "spi.hpp"

static const uint32_t PLATFORM_SPI_CORE_CLOCK_HZ = 84000000U;
static const uint32_t PLATFORM_SPI_DEFAULT_CLOCK_HZ = 4000000U;

static uint8_t platform_reverse_bits(uint8_t value)
{
    value = static_cast<uint8_t>(((value & 0xF0U) >> 4) | ((value & 0x0FU) << 4));
    value = static_cast<uint8_t>(((value & 0xCCU) >> 2) | ((value & 0x33U) << 2));
    value = static_cast<uint8_t>(((value & 0xAAU) >> 1) | ((value & 0x55U) << 1));
    return value;
}

SPISettings::SPISettings()
    : clock_hz_(PLATFORM_SPI_DEFAULT_CLOCK_HZ),
      bit_order_(MSBFIRST),
      data_mode_(SPI_MODE0)
{
}

SPISettings::SPISettings(uint32_t clock_hz, uint8_t bit_order, uint8_t data_mode)
    : clock_hz_(clock_hz),
      bit_order_(bit_order),
      data_mode_(data_mode)
{
}

uint32_t SPISettings::clock_hz(void) const
{
    return clock_hz_;
}

uint8_t SPISettings::bit_order(void) const
{
    return bit_order_;
}

uint8_t SPISettings::data_mode(void) const
{
    return data_mode_;
}

SPIClass::SPIClass()
    : settings_(),
      initialized_(0U)
{
}

void SPIClass::begin(void)
{
    hal_spi_begin();
    apply_current_settings();
    initialized_ = 1U;
}

void SPIClass::end(void)
{
    hal_spi_end();
    initialized_ = 0U;
}

void SPIClass::beginTransaction(const SPISettings& settings)
{
    settings_ = settings;

    if (initialized_ != 0U)
    {
        apply_current_settings();
    }
}

void SPIClass::endTransaction(void)
{
}

uint8_t SPIClass::transfer(uint8_t value)
{
    if (initialized_ == 0U)
    {
        return 0U;
    }

    if (settings_.bit_order() == LSBFIRST)
    {
        const uint8_t tx_value = platform_reverse_bits(value);
        const uint8_t rx_value = hal_spi_transfer(tx_value);
        return platform_reverse_bits(rx_value);
    }

    return hal_spi_transfer(value);
}

uint16_t SPIClass::transfer16(uint16_t value)
{
    uint16_t result = 0U;

    if (settings_.bit_order() == LSBFIRST)
    {
        result = static_cast<uint16_t>(transfer(static_cast<uint8_t>(value & 0xFFU)));
        result |= static_cast<uint16_t>(transfer(static_cast<uint8_t>((value >> 8) & 0xFFU))) << 8;
    }
    else
    {
        result = static_cast<uint16_t>(transfer(static_cast<uint8_t>((value >> 8) & 0xFFU))) << 8;
        result |= static_cast<uint16_t>(transfer(static_cast<uint8_t>(value & 0xFFU)));
    }

    return result;
}

void SPIClass::transfer(void* buffer, size_t size)
{
    if (buffer == 0)
    {
        return;
    }

    uint8_t* data = static_cast<uint8_t*>(buffer);

    for (size_t index = 0U; index < size; ++index)
    {
        data[index] = transfer(data[index]);
    }
}

void SPIClass::setClockDivider(uint8_t divider)
{
    if (divider == 0U)
    {
        divider = SPI_CLOCK_DIV128;
    }

    settings_ = SPISettings(PLATFORM_SPI_CORE_CLOCK_HZ / divider,
                            settings_.bit_order(),
                            settings_.data_mode());

    if (initialized_ != 0U)
    {
        apply_current_settings();
    }
}

void SPIClass::setBitOrder(uint8_t bit_order)
{
    settings_ = SPISettings(settings_.clock_hz(),
                            bit_order,
                            settings_.data_mode());

    if (initialized_ != 0U)
    {
        apply_current_settings();
    }
}

void SPIClass::setDataMode(uint8_t data_mode)
{
    settings_ = SPISettings(settings_.clock_hz(),
                            settings_.bit_order(),
                            data_mode);

    if (initialized_ != 0U)
    {
        apply_current_settings();
    }
}

HalSpiConfig SPIClass::make_hal_config(const SPISettings& settings) const
{
    HalSpiConfig config;
    config.clock_hz = settings.clock_hz();
    config.bit_order = HAL_SPI_MSB_FIRST;
    config.mode = convert_data_mode(settings.data_mode());
    return config;
}

HalSpiBitOrder SPIClass::convert_bit_order(uint8_t bit_order) const
{
    return (bit_order == LSBFIRST) ? HAL_SPI_LSB_FIRST : HAL_SPI_MSB_FIRST;
}

HalSpiMode SPIClass::convert_data_mode(uint8_t data_mode) const
{
    switch (data_mode)
    {
        case SPI_MODE1:
            return HAL_SPI_MODE_1;

        case SPI_MODE2:
            return HAL_SPI_MODE_2;

        case SPI_MODE3:
            return HAL_SPI_MODE_3;

        case SPI_MODE0:
        default:
            return HAL_SPI_MODE_0;
    }
}

void SPIClass::apply_current_settings(void)
{
    const HalSpiConfig config = make_hal_config(settings_);
    hal_spi_configure(config);
}
