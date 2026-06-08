
/**
 * @file sam3x_rtc.hpp
 * @brief HAL локального RTC ATSAM3X8E.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Дата и время локального RTC.
 */
struct Sam3xRtcDateTime
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t day_of_week;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

/**
 * @brief Результат операции RTC.
 */
enum Sam3xRtcResult
{
    SAM3X_RTC_OK = 0,
    SAM3X_RTC_INVALID_ARGUMENT = 1,
    SAM3X_RTC_INVALID_DATE_TIME = 2,
    SAM3X_RTC_ACK_TIMEOUT = 3,
    SAM3X_RTC_VERIFY_FAILED = 4,
    SAM3X_RTC_BUSY = 5
};

/**
 * @brief Состояние неблокирующей записи RTC.
 */
enum Sam3xRtcUpdateState
{
    SAM3X_RTC_UPDATE_IDLE = 0,
    SAM3X_RTC_UPDATE_WAIT_ACK = 1,
    SAM3X_RTC_UPDATE_DONE = 2,
    SAM3X_RTC_UPDATE_FAILED = 3
};

/**
 * @brief Инициализирует RTC в 24-часовом режиме и запрашивает внешний slow clock.
 */
void sam3x_rtc_init(void);

/**
 * @brief Запрашивает переключение slow clock на внешний 32.768 kHz кварц.
 */
void sam3x_rtc_request_external_32k(void);

/**
 * @brief Возвращает признак выбранного внешнего 32.768 kHz кварца.
 */
uint8_t sam3x_rtc_external_32k_selected(void);

/**
 * @brief Считывает текущие дату и время.
 */
Sam3xRtcResult sam3x_rtc_get_datetime(Sam3xRtcDateTime* date_time);

/**
 * @brief Устанавливает дату и время блокирующей процедурой.
 */
Sam3xRtcResult sam3x_rtc_set_datetime(const Sam3xRtcDateTime* date_time);

/**
 * @brief Запускает неблокирующую установку даты и времени.
 */
Sam3xRtcResult sam3x_rtc_start_datetime_update(const Sam3xRtcDateTime* date_time);

/**
 * @brief Обслуживает неблокирующую установку даты и времени.
 */
void sam3x_rtc_poll_datetime_update(void);

/**
 * @brief Прерывает неблокирующую установку даты и времени.
 */
void sam3x_rtc_cancel_datetime_update(Sam3xRtcResult result);

/**
 * @brief Возвращает состояние неблокирующей установки даты и времени.
 */
Sam3xRtcUpdateState sam3x_rtc_update_state(void);

/**
 * @brief Возвращает результат последней неблокирующей установки даты и времени.
 */
Sam3xRtcResult sam3x_rtc_update_result(void);

/**
 * @brief Возвращает признак активной неблокирующей установки даты и времени.
 */
uint8_t sam3x_rtc_update_busy(void);

/**
 * @brief Возвращает регистр RTC_SR.
 */
uint32_t sam3x_rtc_status_register(void);

/**
 * @brief Возвращает регистр RTC_VER.
 */
uint32_t sam3x_rtc_valid_entry_register(void);

/**
 * @brief Возвращает регистр RTC_TIMR.
 */
uint32_t sam3x_rtc_time_register(void);

/**
 * @brief Возвращает регистр RTC_CALR.
 */
uint32_t sam3x_rtc_calendar_register(void);

/**
 * @brief Возвращает признак корректного состояния регистров времени и даты.
 */
uint8_t sam3x_rtc_registers_valid(void);

/**
 * @brief Возвращает признак допустимого диапазона считанных даты и времени.
 */
uint8_t sam3x_rtc_datetime_range_valid(const Sam3xRtcDateTime* date_time);

/**
 * @brief Нормализует год к диапазону 2000..2099.
 *
 * Значения 0..99 трактуются как 2000..2099.
 */
uint16_t sam3x_rtc_normalize_year(uint16_t year);

/**
 * @brief Вычисляет день недели для даты.
 *
 * @return Значение 1..7.
 */
uint8_t sam3x_rtc_calculate_day_of_week(uint16_t year, uint8_t month, uint8_t day);

/**
 * @brief Возвращает текстовое описание результата.
 */
const char* sam3x_rtc_result_text(Sam3xRtcResult result);

/**
 * @brief Возвращает текстовое описание состояния неблокирующей записи RTC.
 */
const char* sam3x_rtc_update_state_text(Sam3xRtcUpdateState state);
