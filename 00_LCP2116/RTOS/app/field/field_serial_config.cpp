/**
 * @file field_serial_config.cpp
 * @brief Реализация чтения канонических baud.TXT и Parity.TXT.
 */

#include "field_serial_config.hpp"

#include <stddef.h>
#include <stdint.h>

namespace
{
static const char BAUD_FILE_NAME[] = "baud.TXT";
static const char PARITY_FILE_NAME[] = "Parity.TXT";
static const uint8_t EXPECTED_VALUE_COUNT = LCP_FIELD_PORT_COUNT;
static const uint32_t MINIMUM_BAUDRATE = 300U;
static const uint32_t MAXIMUM_BAUDRATE = 1000000U;

SdConfigResult load_baud(
    LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT])
{
    uint32_t values[LCP_FIELD_PORT_COUNT + 1U];
    uint8_t loaded_count = 0U;
    const SdConfigResult result = sd_config_load_uint32(
        BAUD_FILE_NAME,
        values,
        sizeof(values) / sizeof(values[0]),
        &loaded_count);

    if (result != SD_CONFIG_OK)
    {
        return result;
    }

    if ((loaded_count != EXPECTED_VALUE_COUNT) ||
        (values[0] != EXPECTED_VALUE_COUNT))
    {
        return SD_CONFIG_INVALID_COUNT;
    }

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        const uint32_t baudrate = values[index + 1U];

        if ((baudrate < MINIMUM_BAUDRATE) ||
            (baudrate > MAXIMUM_BAUDRATE))
        {
            return SD_CONFIG_INVALID_VALUE;
        }
    }

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        configs[index].baudrate = values[index + 1U];
    }

    return SD_CONFIG_OK;
}

uint8_t parity_frame(int16_t code, HalUartFrame* frame)
{
    if (frame == 0)
    {
        return 0U;
    }

    switch (code)
    {
        case 0:
            *frame = HAL_UART_FRAME_8N1;
            return 1U;
        case 1:
            *frame = HAL_UART_FRAME_8O1;
            return 1U;
        case 2:
            *frame = HAL_UART_FRAME_8E1;
            return 1U;
        default:
            return 0U;
    }
}

SdConfigResult load_parity(
    LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT])
{
    int16_t values[LCP_FIELD_PORT_COUNT + 1U];
    HalUartFrame frames[LCP_FIELD_PORT_COUNT];
    uint8_t loaded_count = 0U;
    const SdConfigResult result = sd_config_load_int16(
        PARITY_FILE_NAME,
        values,
        sizeof(values) / sizeof(values[0]),
        &loaded_count);

    if (result != SD_CONFIG_OK)
    {
        return result;
    }

    if ((loaded_count != EXPECTED_VALUE_COUNT) ||
        (values[0] != EXPECTED_VALUE_COUNT))
    {
        return SD_CONFIG_INVALID_COUNT;
    }

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        if (parity_frame(values[index + 1U], &frames[index]) == 0U)
        {
            return SD_CONFIG_INVALID_VALUE;
        }
    }

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        configs[index].frame = frames[index];
    }

    return SD_CONFIG_OK;
}
}

void field_serial_config_set_defaults(
    LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT])
{
    if (configs == 0)
    {
        return;
    }

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        configs[index].baudrate = 9600U;
        configs[index].frame = HAL_UART_FRAME_8N1;
    }
}

void field_serial_config_load(
    LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT],
    FieldSerialConfigReport* report)
{
    if ((configs == 0) || (report == 0))
    {
        return;
    }

    report->baud_result = SD_CONFIG_NOT_ATTEMPTED;
    report->parity_result = SD_CONFIG_NOT_ATTEMPTED;
    report->loaded_from_sd = 0U;

    LcpFieldPortConfig staged[LCP_FIELD_PORT_COUNT];

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        staged[index] = configs[index];
    }

    report->baud_result = load_baud(staged);

    if (report->baud_result == SD_CONFIG_OK)
    {
        for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
        {
            configs[index].baudrate = staged[index].baudrate;
        }

        report->loaded_from_sd = 1U;
    }

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        staged[index] = configs[index];
    }

    report->parity_result = load_parity(staged);

    if (report->parity_result == SD_CONFIG_OK)
    {
        for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
        {
            configs[index].frame = staged[index].frame;
        }

        report->loaded_from_sd = 1U;
    }
}
