/**
 * @file rtc_status.cpp
 * @brief Неблокирующая диагностика и настройка локального RTC ATSAM3X8E.
 *
 * Команда `rtc set` только запускает аппаратную последовательность обновления.
 * ACKUPD, запись регистров, проверка результата и таймаут обслуживаются из
 * rtc_status_poll(). Это принципиально важно: service console работает внутри
 * общей LCP task и не должна останавливать X2X, FieldSensor или Ethernet.
 *
 * При добавлении нового формата даты изменяйте только parse_datetime_value().
 * Аппаратную state machine следует оставлять в `sam3x_rtc.*`.
 */

#include "rtc_status.hpp"
#include "diagnostic_output.hpp"

#include "../../hal/sam3x_rtc.hpp"
#include "../../platform/platform.hpp"

#include <stddef.h>
#include <string.h>

namespace
{
constexpr uint32_t RTC_UPDATE_TIMEOUT_MS = 2500U;

uint32_t g_update_start_ms = 0U;

uint8_t is_space(char value)
{
    return ((value == ' ') || (value == '\t')) ? 1U : 0U;
}

const char* skip_spaces(const char* text)
{
    while ((text != 0) && (is_space(*text) != 0U))
    {
        ++text;
    }

    return text;
}

uint8_t parse_digits(const char*& cursor,
                     uint8_t digit_count,
                     uint16_t* output)
{
    if ((cursor == 0) || (output == 0) || (digit_count == 0U))
    {
        return 0U;
    }

    uint16_t value = 0U;

    for (uint8_t index = 0U; index < digit_count; ++index)
    {
        const char character = cursor[index];

        if ((character < '0') || (character > '9'))
        {
            return 0U;
        }

        value = static_cast<uint16_t>(
            (value * 10U) + static_cast<uint16_t>(character - '0'));
    }

    cursor += digit_count;
    *output = value;
    return 1U;
}

uint8_t consume_separator(const char*& cursor, char first, char second)
{
    if ((cursor == 0) || ((*cursor != first) && (*cursor != second)))
    {
        return 0U;
    }

    ++cursor;
    return 1U;
}

uint8_t parse_year(const char*& cursor, uint16_t* year)
{
    if ((cursor == 0) || (year == 0))
    {
        return 0U;
    }

    const char* probe = cursor;
    uint16_t parsed = 0U;

    if ((parse_digits(probe, 4U, &parsed) != 0U) &&
        ((*probe == '-') || (*probe == '.')))
    {
        cursor = probe;
        *year = sam3x_rtc_normalize_year(parsed);
        return 1U;
    }

    probe = cursor;

    if ((parse_digits(probe, 2U, &parsed) != 0U) &&
        ((*probe == '-') || (*probe == '.')))
    {
        cursor = probe;
        *year = static_cast<uint16_t>(2000U + parsed);
        return 1U;
    }

    return 0U;
}

uint8_t parse_datetime_value(const char* text,
                             Sam3xRtcDateTime* date_time)
{
    if ((text == 0) || (date_time == 0))
    {
        return 0U;
    }

    const char* cursor = skip_spaces(text);
    uint16_t value = 0U;

    if ((parse_year(cursor, &date_time->year) == 0U) ||
        (consume_separator(cursor, '-', '.') == 0U) ||
        (parse_digits(cursor, 2U, &value) == 0U))
    {
        return 0U;
    }
    date_time->month = static_cast<uint8_t>(value);

    if ((consume_separator(cursor, '-', '.') == 0U) ||
        (parse_digits(cursor, 2U, &value) == 0U))
    {
        return 0U;
    }
    date_time->day = static_cast<uint8_t>(value);

    if ((*cursor != ' ') && (*cursor != 't'))
    {
        return 0U;
    }
    ++cursor;
    cursor = skip_spaces(cursor);

    if (parse_digits(cursor, 2U, &value) == 0U)
    {
        return 0U;
    }
    date_time->hour = static_cast<uint8_t>(value);

    if ((*cursor != ':') ||
        (++cursor, parse_digits(cursor, 2U, &value) == 0U))
    {
        return 0U;
    }
    date_time->minute = static_cast<uint8_t>(value);

    if ((*cursor != ':') ||
        (++cursor, parse_digits(cursor, 2U, &value) == 0U))
    {
        return 0U;
    }
    date_time->second = static_cast<uint8_t>(value);

    cursor = skip_spaces(cursor);

    if (*cursor != '\0')
    {
        return 0U;
    }

    date_time->day_of_week = sam3x_rtc_calculate_day_of_week(
        date_time->year,
        date_time->month,
        date_time->day);

    return (sam3x_rtc_datetime_range_valid(date_time) != 0U) ? 1U : 0U;
}

void print_2_digits(uint8_t value)
{
    if (value < 10U)
    {
        SerialUSB.print("0");
    }

    SerialUSB.print(static_cast<int>(value));
}

void print_4_digits(uint16_t value)
{
    const uint16_t normalized = sam3x_rtc_normalize_year(value);

    if (normalized < 1000U)
    {
        SerialUSB.print("0");
    }
    if (normalized < 100U)
    {
        SerialUSB.print("0");
    }
    if (normalized < 10U)
    {
        SerialUSB.print("0");
    }

    SerialUSB.print(static_cast<unsigned long>(normalized));
}

void print_datetime(const Sam3xRtcDateTime& date_time)
{
    print_4_digits(date_time.year);
    SerialUSB.print("-");
    print_2_digits(date_time.month);
    SerialUSB.print("-");
    print_2_digits(date_time.day);
    SerialUSB.print(" ");
    print_2_digits(date_time.hour);
    SerialUSB.print(":");
    print_2_digits(date_time.minute);
    SerialUSB.print(":");
    print_2_digits(date_time.second);
}

void service_rtc_update_timeout(void)
{
    if (sam3x_rtc_update_busy() == 0U)
    {
        return;
    }

    const uint32_t now_ms = millis();

    if ((uint32_t)(now_ms - g_update_start_ms) >= RTC_UPDATE_TIMEOUT_MS)
    {
        sam3x_rtc_cancel_datetime_update(SAM3X_RTC_ACK_TIMEOUT);
    }
}
}

void rtc_status_init(void)
{
    sam3x_rtc_init();
    g_update_start_ms = 0U;
}

void rtc_status_poll(void)
{
    sam3x_rtc_poll_datetime_update();
    service_rtc_update_timeout();
}

void rtc_status_print_report(void)
{
    Sam3xRtcDateTime now = {};

    rtc_status_poll();

    const Sam3xRtcResult read_result = sam3x_rtc_get_datetime(&now);
    const uint8_t register_valid = sam3x_rtc_registers_valid();
    const uint8_t range_valid = (read_result == SAM3X_RTC_OK) ?
        sam3x_rtc_datetime_range_valid(&now) : 0U;

    diagnostic_print_section("LOCAL RTC");

    diagnostic_print_group("Clock and update state");
    SerialUSB.print("slow_clock_source=");
    SerialUSB.print(sam3x_rtc_external_32k_selected() ?
                    "external 32.768 kHz crystal" :
                    "internal slow clock");
    SerialUSB.print("\r\nupdate_state=");
    SerialUSB.print(sam3x_rtc_update_state_text(sam3x_rtc_update_state()));
    SerialUSB.print(", update_result=");
    SerialUSB.print(sam3x_rtc_result_text(sam3x_rtc_update_result()));
    SerialUSB.print(", timeout_ms=");
    SerialUSB.print(static_cast<unsigned long>(RTC_UPDATE_TIMEOUT_MS));
    SerialUSB.print("\r\n");

    diagnostic_print_group("Current date and time");
    SerialUSB.print("read_result=");
    SerialUSB.print(sam3x_rtc_result_text(read_result));
    SerialUSB.print("\r\ndatetime=");

    if (read_result == SAM3X_RTC_OK)
    {
        print_datetime(now);
        SerialUSB.print("\r\nday_of_week=");
        SerialUSB.print(static_cast<int>(now.day_of_week));
    }
    else
    {
        SerialUSB.print("unavailable");
    }

    SerialUSB.print("\r\nregisters_valid=");
    SerialUSB.print((register_valid != 0U) ? "yes" : "no");
    SerialUSB.print(", range_valid=");
    SerialUSB.print((range_valid != 0U) ? "yes" : "no");
    SerialUSB.print("\r\nmessage=");
    SerialUSB.print(((register_valid != 0U) && (range_valid != 0U)) ?
                    "local RTC date/time valid" :
                    "set local RTC date/time");
    SerialUSB.print("\r\n");

    diagnostic_print_group("Raw registers");
    SerialUSB.print("RTC_SR=0x");
    SerialUSB.print(static_cast<unsigned long>(sam3x_rtc_status_register()), HEX);
    SerialUSB.print(", RTC_VER=0x");
    SerialUSB.print(static_cast<unsigned long>(sam3x_rtc_valid_entry_register()), HEX);
    SerialUSB.print("\r\nRTC_TIMR=0x");
    SerialUSB.print(static_cast<unsigned long>(sam3x_rtc_time_register()), HEX);
    SerialUSB.print(", RTC_CALR=0x");
    SerialUSB.print(static_cast<unsigned long>(sam3x_rtc_calendar_register()), HEX);
    SerialUSB.print("\r\n");
}

uint8_t rtc_status_handle_command(const char* command)
{
    if (command == 0)
    {
        return 0U;
    }

    if ((strcmp(command, "rtc") == 0) ||
        (strcmp(command, "rtc status") == 0))
    {
        rtc_status_print_report();
        return 1U;
    }

    static const char RTC_SET_PREFIX[] = "rtc set ";

    if (strncmp(command,
                RTC_SET_PREFIX,
                sizeof(RTC_SET_PREFIX) - 1U) == 0)
    {
        Sam3xRtcDateTime date_time = {};
        const char* value = command + sizeof(RTC_SET_PREFIX) - 1U;

        if (parse_datetime_value(value, &date_time) == 0U)
        {
            SerialUSB.print("rtc set failed: invalid date/time\r\n");
            SerialUSB.print("usage: rtc set YYYY-MM-DD HH:MM:SS\r\n");
            SerialUSB.print("accepted: four-digit or two-digit year; '-' or '.' date separators; space or T\r\n");
            return 1U;
        }

        const Sam3xRtcResult start_result =
            sam3x_rtc_start_datetime_update(&date_time);

        SerialUSB.print("rtc requested_datetime=");
        print_datetime(date_time);
        SerialUSB.print("\r\nrtc update_start_result=");
        SerialUSB.print(sam3x_rtc_result_text(start_result));
        SerialUSB.print("\r\n");

        if (start_result == SAM3X_RTC_OK)
        {
            g_update_start_ms = millis();
            SerialUSB.print("rtc update is nonblocking; use 'rtc' to inspect completion\r\n");
        }

        return 1U;
    }

    return 0U;
}
