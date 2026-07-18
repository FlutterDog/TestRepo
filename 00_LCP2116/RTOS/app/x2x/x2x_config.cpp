/**
 * @file x2x_config.cpp
 * @brief Реализация безопасного парсера X2X.TXT.
 */

#include "x2x_config.hpp"
#include "x2x_catalog.hpp"
#include "../../libs/lcp_sd_storage/lcp_sd_storage.hpp"

#include <stddef.h>
#include <stdint.h>

namespace
{
static const uint16_t X2X_CONFIG_LINE_CAPACITY = 64U;

uint8_t is_space(char value)
{
    return ((value == ' ') || (value == '\t') ||
            (value == '\r') || (value == '\n')) ? 1U : 0U;
}

char ascii_to_lower(char value)
{
    if ((value >= 'A') && (value <= 'Z'))
    {
        return static_cast<char>(value + ('a' - 'A'));
    }

    return value;
}

uint8_t text_equals_ignore_case(const char* left, const char* right)
{
    if ((left == 0) || (right == 0))
    {
        return 0U;
    }

    while ((*left != '\0') && (*right != '\0'))
    {
        if (ascii_to_lower(*left) != ascii_to_lower(*right))
        {
            return 0U;
        }

        ++left;
        ++right;
    }

    return ((*left == '\0') && (*right == '\0')) ? 1U : 0U;
}

void strip_comment_and_trim(char* line)
{
    uint16_t index = 0U;

    while (line[index] != '\0')
    {
        if ((line[index] == '/') && (line[index + 1U] == '/'))
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
            line[destination] = line[start];
            ++destination;
            ++start;
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
        line[length - 1U] = '\0';
        --length;
    }
}

uint8_t parse_decimal(const char* text, int32_t* result)
{
    uint16_t index = 0U;
    int32_t sign = 1L;
    int32_t value = 0L;
    uint8_t digit_found = 0U;

    if ((text == 0) || (result == 0))
    {
        return 0U;
    }

    if (text[index] == '-')
    {
        sign = -1L;
        ++index;
    }
    else if (text[index] == '+')
    {
        ++index;
    }

    while (text[index] != '\0')
    {
        if ((text[index] < '0') || (text[index] > '9'))
        {
            return 0U;
        }

        digit_found = 1U;
        value = (value * 10L) + static_cast<int32_t>(text[index] - '0');

        if (value > 65535L)
        {
            return 0U;
        }

        ++index;
    }

    if (digit_found == 0U)
    {
        return 0U;
    }

    *result = sign * value;
    return 1U;
}

uint8_t read_physical_line(LcpSdReadFile& file,
                           char* line,
                           uint16_t capacity,
                           uint8_t* line_too_long)
{
    uint16_t length = 0U;
    int value = -1;

    *line_too_long = 0U;

    while ((value = lcp_sd_storage_read_byte(&file)) >= 0)
    {
        const char character = static_cast<char>(value);

        if (character == '\n')
        {
            break;
        }

        if (length < (capacity - 1U))
        {
            line[length] = character;
            ++length;
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

X2XConfigResult fail_and_close(LcpSdReadFile& file,
                               X2XConfigError& error,
                               X2XConfigResult result,
                               uint16_t line,
                               int32_t value)
{
    lcp_sd_storage_close_read(&file);
    error.result = result;
    error.physical_line = line;
    error.value = value;
    return result;
}
}

void x2x_config_reset(X2XConfig& config, X2XConfigError& error)
{
    config.module_count = 0U;

    for (uint8_t index = 0U; index < X2X_MAX_MODULES; ++index)
    {
        config.module_types[index] = X2X_DEVICE_LCP;
    }

    error.result = X2X_CONFIG_OK;
    error.physical_line = 0U;
    error.value = 0L;
}

X2XConfigResult x2x_config_load(const char* file_name,
                                X2XConfig& config,
                                X2XConfigError& error)
{
    LcpSdReadFile file;
    char line[X2X_CONFIG_LINE_CAPACITY];
    uint8_t line_too_long = 0U;
    uint8_t count_read = 0U;
    uint8_t module_index = 0U;
    uint8_t fin_seen = 0U;
    uint16_t physical_line = 0U;

    x2x_config_reset(config, error);

    if (lcp_sd_storage_ready() == 0U)
    {
        error.result = X2X_CONFIG_STORAGE_NOT_READY;
        return error.result;
    }

    const LcpSdStorageResult open_result = lcp_sd_storage_open_read(file_name, &file);

    if (open_result == LCP_SD_STORAGE_FILE_NOT_FOUND)
    {
        error.result = X2X_CONFIG_FILE_NOT_FOUND;
        return error.result;
    }

    if (open_result != LCP_SD_STORAGE_OK)
    {
        error.result = X2X_CONFIG_FILE_OPEN_FAILED;
        return error.result;
    }

    while (read_physical_line(file,
                              line,
                              X2X_CONFIG_LINE_CAPACITY,
                              &line_too_long) != 0U)
    {
        ++physical_line;

        if (line_too_long != 0U)
        {
            return fail_and_close(file,
                                  error,
                                  X2X_CONFIG_LINE_TOO_LONG,
                                  physical_line,
                                  0L);
        }

        strip_comment_and_trim(line);

        if (line[0] == '\0')
        {
            continue;
        }

        if (text_equals_ignore_case(line, "Fin") != 0U)
        {
            if ((count_read == 0U) || (module_index != config.module_count))
            {
                return fail_and_close(file,
                                      error,
                                      X2X_CONFIG_INCOMPLETE_FILE,
                                      physical_line,
                                      module_index);
            }

            fin_seen = 1U;
            continue;
        }

        if (fin_seen != 0U)
        {
            return fail_and_close(file,
                                  error,
                                  X2X_CONFIG_EXTRA_DATA,
                                  physical_line,
                                  0L);
        }

        int32_t parsed_value = 0L;

        if (parse_decimal(line, &parsed_value) == 0U)
        {
            return fail_and_close(file,
                                  error,
                                  X2X_CONFIG_INVALID_VALUE,
                                  physical_line,
                                  0L);
        }

        if (count_read == 0U)
        {
            if ((parsed_value < 0L) ||
                (parsed_value > static_cast<int32_t>(X2X_MAX_MODULES)))
            {
                return fail_and_close(file,
                                      error,
                                      X2X_CONFIG_INVALID_COUNT,
                                      physical_line,
                                      parsed_value);
            }

            config.module_count = static_cast<uint8_t>(parsed_value);
            count_read = 1U;
            continue;
        }

        if (module_index >= config.module_count)
        {
            return fail_and_close(file,
                                  error,
                                  X2X_CONFIG_EXTRA_DATA,
                                  physical_line,
                                  parsed_value);
        }

        const X2XModuleDescriptor* descriptor =
            (parsed_value >= 0L)
                ? x2x_catalog_find_by_id(static_cast<uint16_t>(parsed_value))
                : 0;

        if ((descriptor == 0) || (descriptor->allowed_in_x2x_config == 0U))
        {
            return fail_and_close(file,
                                  error,
                                  X2X_CONFIG_INVALID_DEVICE_ID,
                                  physical_line,
                                  parsed_value);
        }

        config.module_types[module_index] = descriptor->type;
        ++module_index;
    }

    lcp_sd_storage_close_read(&file);

    if (count_read == 0U)
    {
        error.result = X2X_CONFIG_EMPTY_FILE;
        return error.result;
    }

    if (module_index != config.module_count)
    {
        error.result = X2X_CONFIG_INCOMPLETE_FILE;
        error.physical_line = physical_line;
        error.value = module_index;
        return error.result;
    }

    error.result = X2X_CONFIG_OK;
    return X2X_CONFIG_OK;
}

const char* x2x_config_result_text(X2XConfigResult result)
{
    switch (result)
    {
        case X2X_CONFIG_OK:
            return "ok";

        case X2X_CONFIG_STORAGE_NOT_READY:
            return "storage not ready";

        case X2X_CONFIG_FILE_NOT_FOUND:
            return "file not found";

        case X2X_CONFIG_FILE_OPEN_FAILED:
            return "file open failed";

        case X2X_CONFIG_EMPTY_FILE:
            return "empty file";

        case X2X_CONFIG_INVALID_COUNT:
            return "invalid module count";

        case X2X_CONFIG_LINE_TOO_LONG:
            return "line too long";

        case X2X_CONFIG_INVALID_VALUE:
            return "invalid numeric value";

        case X2X_CONFIG_INVALID_DEVICE_ID:
            return "invalid X2X device ID";

        case X2X_CONFIG_INCOMPLETE_FILE:
            return "incomplete file";

        case X2X_CONFIG_EXTRA_DATA:
            return "extra data";

        default:
            return "unknown";
    }
}
