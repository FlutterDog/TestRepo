/**
 * @file field_serial_config.cpp
 * @brief Реализация чтения baud.TXT и Parity.TXT.
 */

#include "field_serial_config.hpp"

#include "../../libs/lcp_sd_storage/lcp_sd_storage.hpp"

#include <stddef.h>
#include <stdint.h>

namespace
{
static const char* const BAUD_FILE_NAME = "baud.TXT";
static const char* const PARITY_FILE_NAME = "Parity.TXT";
static const uint16_t LINE_CAPACITY = 32U;
static const uint8_t EXPECTED_VALUE_COUNT = LCP_FIELD_PORT_COUNT;

char g_line[LINE_CAPACITY];

uint8_t is_space(char value)
{
    return ((value == ' ') || (value == '\t') ||
            (value == '\r') || (value == '\n')) ? 1U : 0U;
}

void strip_comment_and_trim(char* line)
{
    uint16_t index = 0U;

    while (line[index] != '\0')
    {
        if (((line[index] == '/') && (line[index + 1U] == '/')) ||
            (line[index] == '#'))
        {
            line[index] = '\0';
            break;
        }

        ++index;
    }

    uint16_t start = 0U;

    while ((line[start] != '\0') && (is_space(line[start]) != 0U))
    {
        ++start;
    }

    if (start != 0U)
    {
        uint16_t destination = 0U;

        while (line[start] != '\0')
        {
            line[destination++] = line[start++];
        }

        line[destination] = '\0';
    }

    uint16_t length = 0U;

    while (line[length] != '\0')
    {
        ++length;
    }

    while ((length > 0U) && (is_space(line[length - 1U]) != 0U))
    {
        line[--length] = '\0';
    }
}

uint8_t read_line(LcpSdReadFile* file,
                  char* line,
                  uint16_t capacity,
                  uint8_t* line_too_long)
{
    uint16_t length = 0U;
    int value = -1;
    *line_too_long = 0U;

    while ((value = lcp_sd_storage_read_byte(file)) >= 0)
    {
        const char character = static_cast<char>(value);

        if (character == '\n')
        {
            break;
        }

        if (length < (capacity - 1U))
        {
            line[length++] = character;
        }
        else
        {
            *line_too_long = 1U;
        }
    }

    if ((value < 0) && (length == 0U) && (*line_too_long == 0U))
    {
        return 0U;
    }

    line[length] = '\0';
    return 1U;
}

uint8_t parse_uint32(const char* text, uint32_t* output)
{
    uint32_t value = 0U;
    uint16_t index = 0U;
    uint8_t digit_found = 0U;

    if ((text == 0) || (output == 0))
    {
        return 0U;
    }

    if (text[index] == '+')
    {
        ++index;
    }

    while (text[index] != '\0')
    {
        if ((text[index] < '0') || (text[index] > '9'))
        {
            return 0U;
        }

        const uint32_t digit = static_cast<uint32_t>(text[index] - '0');

        if (value > ((0xFFFFFFFFUL - digit) / 10UL))
        {
            return 0U;
        }

        value = (value * 10UL) + digit;
        digit_found = 1U;
        ++index;
    }

    if (digit_found == 0U)
    {
        return 0U;
    }

    *output = value;
    return 1U;
}

uint8_t parse_parity(const char* text, HalUartFrame* frame)
{
    uint32_t numeric = 0U;

    if ((text == 0) || (frame == 0) ||
        (parse_uint32(text, &numeric) == 0U))
    {
        return 0U;
    }

    switch (numeric)
    {
        case 0U:
            *frame = HAL_UART_FRAME_8N1;
            return 1U;
        case 1U:
            *frame = HAL_UART_FRAME_8O1;
            return 1U;
        case 2U:
            *frame = HAL_UART_FRAME_8E1;
            return 1U;
        default:
            return 0U;
    }
}

FieldSerialConfigResult storage_open_result(LcpSdStorageResult result)
{
    if (result == LCP_SD_STORAGE_FILE_NOT_FOUND)
    {
        return FIELD_SERIAL_CONFIG_FILE_NOT_FOUND;
    }

    return (result == LCP_SD_STORAGE_OK) ?
        FIELD_SERIAL_CONFIG_OK : FIELD_SERIAL_CONFIG_FILE_OPEN_FAILED;
}

FieldSerialConfigResult load_baud(
    LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT])
{
    LcpSdReadFile file;
    const LcpSdStorageResult open_result =
        lcp_sd_storage_open_read(BAUD_FILE_NAME, &file);
    const FieldSerialConfigResult translated = storage_open_result(open_result);

    if (translated != FIELD_SERIAL_CONFIG_OK)
    {
        return translated;
    }

    uint8_t significant_line = 0U;
    uint8_t value_index = 0U;
    uint8_t line_too_long = 0U;

    while (read_line(&file, g_line, LINE_CAPACITY, &line_too_long) != 0U)
    {
        if (line_too_long != 0U)
        {
            lcp_sd_storage_close_read(&file);
            return FIELD_SERIAL_CONFIG_LINE_TOO_LONG;
        }

        strip_comment_and_trim(g_line);

        if (g_line[0] == '\0')
        {
            continue;
        }

        /* Старый формат завершается строкой fin. После четырёх значений
         * оставшаяся часть файла намеренно не разбирается. */
        if ((significant_line != 0U) &&
            (value_index >= EXPECTED_VALUE_COUNT))
        {
            break;
        }

        uint32_t parsed = 0U;

        if (parse_uint32(g_line, &parsed) == 0U)
        {
            lcp_sd_storage_close_read(&file);
            return FIELD_SERIAL_CONFIG_INVALID_VALUE;
        }

        if (significant_line == 0U)
        {
            if (parsed != EXPECTED_VALUE_COUNT)
            {
                lcp_sd_storage_close_read(&file);
                return FIELD_SERIAL_CONFIG_INVALID_COUNT;
            }
        }
        else
        {
            /* В файле хранится фактическая скорость, а не индекс таблицы. */
            if ((parsed < 300U) || (parsed > 1000000U))
            {
                lcp_sd_storage_close_read(&file);
                return FIELD_SERIAL_CONFIG_INVALID_VALUE;
            }

            configs[value_index].baudrate = parsed;
            ++value_index;
        }

        ++significant_line;
    }

    lcp_sd_storage_close_read(&file);

    if (significant_line == 0U)
    {
        return FIELD_SERIAL_CONFIG_EMPTY_FILE;
    }

    return (value_index == EXPECTED_VALUE_COUNT) ?
        FIELD_SERIAL_CONFIG_OK : FIELD_SERIAL_CONFIG_INCOMPLETE_FILE;
}

FieldSerialConfigResult load_parity(
    LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT])
{
    LcpSdReadFile file;
    const LcpSdStorageResult open_result =
        lcp_sd_storage_open_read(PARITY_FILE_NAME, &file);
    const FieldSerialConfigResult translated = storage_open_result(open_result);

    if (translated != FIELD_SERIAL_CONFIG_OK)
    {
        return translated;
    }

    uint8_t significant_line = 0U;
    uint8_t value_index = 0U;
    uint8_t line_too_long = 0U;

    while (read_line(&file, g_line, LINE_CAPACITY, &line_too_long) != 0U)
    {
        if (line_too_long != 0U)
        {
            lcp_sd_storage_close_read(&file);
            return FIELD_SERIAL_CONFIG_LINE_TOO_LONG;
        }

        strip_comment_and_trim(g_line);

        if (g_line[0] == '\0')
        {
            continue;
        }

        if ((significant_line != 0U) &&
            (value_index >= EXPECTED_VALUE_COUNT))
        {
            break;
        }

        if (significant_line == 0U)
        {
            uint32_t count = 0U;

            if ((parse_uint32(g_line, &count) == 0U) ||
                (count != EXPECTED_VALUE_COUNT))
            {
                lcp_sd_storage_close_read(&file);
                return FIELD_SERIAL_CONFIG_INVALID_COUNT;
            }
        }
        else
        {
            HalUartFrame frame = HAL_UART_FRAME_8N1;

            if (parse_parity(g_line, &frame) == 0U)
            {
                lcp_sd_storage_close_read(&file);
                return FIELD_SERIAL_CONFIG_INVALID_VALUE;
            }

            configs[value_index].frame = frame;
            ++value_index;
        }

        ++significant_line;
    }

    lcp_sd_storage_close_read(&file);

    if (significant_line == 0U)
    {
        return FIELD_SERIAL_CONFIG_EMPTY_FILE;
    }

    return (value_index == EXPECTED_VALUE_COUNT) ?
        FIELD_SERIAL_CONFIG_OK : FIELD_SERIAL_CONFIG_INCOMPLETE_FILE;
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

    report->baud_result = FIELD_SERIAL_CONFIG_NOT_ATTEMPTED;
    report->parity_result = FIELD_SERIAL_CONFIG_NOT_ATTEMPTED;
    report->loaded_from_sd = 0U;

    if (lcp_sd_storage_ready() == 0U)
    {
        report->baud_result = FIELD_SERIAL_CONFIG_CARD_NOT_READY;
        report->parity_result = FIELD_SERIAL_CONFIG_CARD_NOT_READY;
        return;
    }

    LcpFieldPortConfig staged[LCP_FIELD_PORT_COUNT];

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        staged[index] = configs[index];
    }

    report->baud_result = load_baud(staged);

    if (report->baud_result == FIELD_SERIAL_CONFIG_OK)
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

    if (report->parity_result == FIELD_SERIAL_CONFIG_OK)
    {
        for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
        {
            configs[index].frame = staged[index].frame;
        }

        report->loaded_from_sd = 1U;
    }
}

const char* field_serial_config_result_text(FieldSerialConfigResult result)
{
    switch (result)
    {
        case FIELD_SERIAL_CONFIG_NOT_ATTEMPTED:
            return "not attempted";
        case FIELD_SERIAL_CONFIG_OK:
            return "ok";
        case FIELD_SERIAL_CONFIG_CARD_NOT_READY:
            return "card not ready";
        case FIELD_SERIAL_CONFIG_FILE_NOT_FOUND:
            return "file not found";
        case FIELD_SERIAL_CONFIG_FILE_OPEN_FAILED:
            return "file open failed";
        case FIELD_SERIAL_CONFIG_EMPTY_FILE:
            return "empty file";
        case FIELD_SERIAL_CONFIG_INVALID_COUNT:
            return "invalid count";
        case FIELD_SERIAL_CONFIG_INVALID_VALUE:
            return "invalid value";
        case FIELD_SERIAL_CONFIG_INCOMPLETE_FILE:
            return "incomplete file";
        case FIELD_SERIAL_CONFIG_LINE_TOO_LONG:
            return "line too long";
        default:
            return "unknown";
    }
}
