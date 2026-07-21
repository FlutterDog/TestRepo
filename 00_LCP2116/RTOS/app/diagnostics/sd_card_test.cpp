/**
 * @file sd_card_test.cpp
 * @brief Диагностика microSD и проверка базовых файловых операций.
 *
 * Обычный sd_card_test_poll() не блокирует: он выполняет debounce card-detect
 * и не чаще одного раза в секунду повторяет mount. Команда `sd test` является
 * явным сервисным действием и синхронно перезаписывает только `SDTEST.TXT`.
 * Производственные конфигурационные файлы команда не изменяет.
 */

#include "sd_card_test.hpp"
#include "diagnostic_output.hpp"
#include "sd_config.hpp"

#include "../../board/lcp_sd.hpp"
#include "../../libs/lcp_sd_storage/lcp_sd_storage.hpp"
#include "../../platform/platform.hpp"

namespace
{
constexpr uint32_t SD_DETECT_DEBOUNCE_MS = 50U;
constexpr uint32_t SD_INIT_RETRY_MS = 1000U;

uint8_t g_detect_stable = 0U;
uint8_t g_detect_last_raw = 0U;
uint8_t g_detect_debounced = 0U;
uint32_t g_detect_last_change_ms = 0U;
uint32_t g_last_init_attempt_ms = 0U;
uint8_t g_begin_attempted = 0U;
LcpSdStorageResult g_last_storage_result = LCP_SD_STORAGE_NOT_READY;

void update_detect(uint32_t now_ms)
{
    const uint8_t raw_inserted = lcp_sd_card_inserted();

    if (raw_inserted != g_detect_last_raw)
    {
        g_detect_last_raw = raw_inserted;
        g_detect_last_change_ms = now_ms;
        g_detect_stable = 0U;
        return;
    }

    if ((g_detect_stable == 0U) &&
        ((uint32_t)(now_ms - g_detect_last_change_ms) >=
         SD_DETECT_DEBOUNCE_MS))
    {
        g_detect_stable = 1U;
        g_detect_debounced = raw_inserted;

        if (g_detect_debounced == 0U)
        {
            lcp_sd_storage_reset();
            lcp_sd_set_ok_led(0U);
            g_last_storage_result = LCP_SD_STORAGE_NO_CARD;
        }
    }
}
}

void sd_card_test_init(void)
{
    lcp_sd_init_pins();
    lcp_sd_deselect();
    lcp_sd_set_ok_led(0U);

    g_detect_last_raw = lcp_sd_card_inserted();
    g_detect_debounced = g_detect_last_raw;
    g_detect_last_change_ms = millis();
    g_detect_stable = 0U;
    g_last_init_attempt_ms = 0U;
    g_begin_attempted = 0U;
    g_last_storage_result = LCP_SD_STORAGE_NOT_READY;

    sd_card_test_poll();
}

void sd_card_test_poll(void)
{
    const uint32_t now_ms = millis();

    update_detect(now_ms);

    if (g_detect_debounced == 0U)
    {
        return;
    }

    if (lcp_sd_storage_ready() != 0U)
    {
        lcp_sd_set_ok_led(1U);
        return;
    }

    if ((g_begin_attempted != 0U) &&
        ((uint32_t)(now_ms - g_last_init_attempt_ms) < SD_INIT_RETRY_MS))
    {
        return;
    }

    g_begin_attempted = 1U;
    g_last_init_attempt_ms = now_ms;
    g_last_storage_result = lcp_sd_storage_begin();

    lcp_sd_set_ok_led((g_last_storage_result == LCP_SD_STORAGE_OK) ? 1U : 0U);
}

void sd_card_test_print_report(void)
{
    diagnostic_print_section("MICROSD");

    diagnostic_print_group("Hardware");
    SerialUSB.print("cs_pin=23, detect_pin=25, sd_ok_pin=41\r\n");
    SerialUSB.print("detect_raw=");
    SerialUSB.print(static_cast<int>(lcp_sd_detect_raw()));
    SerialUSB.print(", detect_debounced=");
    SerialUSB.print((g_detect_debounced != 0U) ? "inserted" : "not inserted");
    SerialUSB.print(", detect_stable=");
    SerialUSB.print((g_detect_stable != 0U) ? "yes" : "no");
    SerialUSB.print(", debounce_ms=");
    SerialUSB.print(static_cast<unsigned long>(SD_DETECT_DEBOUNCE_MS));
    SerialUSB.print("\r\n");

    diagnostic_print_group("Filesystem");
    SerialUSB.print("init_attempted=");
    SerialUSB.print((g_begin_attempted != 0U) ? "yes" : "no");
    SerialUSB.print(", ready=");
    SerialUSB.print((lcp_sd_storage_ready() != 0U) ? "yes" : "no");
    SerialUSB.print(", last_result=");
    SerialUSB.print(lcp_sd_storage_result_text(g_last_storage_result));
    SerialUSB.print(", retry_period_ms=");
    SerialUSB.print(static_cast<unsigned long>(SD_INIT_RETRY_MS));
    SerialUSB.print("\r\n");

    if (lcp_sd_storage_ready() != 0U)
    {
        SerialUSB.print("fat_type=FAT");
        SerialUSB.print(static_cast<int>(lcp_sd_storage_fat_type()));
        SerialUSB.print(", sectors_per_cluster=");
        SerialUSB.print(static_cast<int>(lcp_sd_storage_sectors_per_cluster()));
        SerialUSB.print(", SDTEST.TXT_exists=");
        SerialUSB.print(lcp_sd_storage_exists("SDTEST.TXT") ? "yes" : "no");
        SerialUSB.print("\r\n");
    }
}

void sd_card_test_run_file_test(void)
{
    int16_t values[4] = { 3, 111, -222, 333 };
    int16_t loaded[4] = { 0, 0, 0, 0 };
    uint8_t loaded_count = 0U;

    diagnostic_print_banner("MICROSD FILE TEST");
    SerialUSB.print("test_file=SDTEST.TXT, action=overwrite then read back\r\n");

    sd_card_test_poll();

    if (lcp_sd_storage_ready() == 0U)
    {
        SerialUSB.print("result=failed, reason=card not ready\r\n");
        return;
    }

    const SdConfigResult save_result =
        sd_config_save_int16("SDTEST.TXT", values, 3U);

    SerialUSB.print("write_result=");
    SerialUSB.print(sd_config_result_text(save_result));
    SerialUSB.print("\r\n");

    if (save_result != SD_CONFIG_OK)
    {
        SerialUSB.print("result=failed\r\n");
        return;
    }

    const SdConfigResult load_result = sd_config_load_int16(
        "SDTEST.TXT",
        loaded,
        sizeof(loaded) / sizeof(loaded[0]),
        &loaded_count);

    SerialUSB.print("read_result=");
    SerialUSB.print(sd_config_result_text(load_result));
    SerialUSB.print(", loaded_count=");
    SerialUSB.print(static_cast<int>(loaded_count));
    SerialUSB.print("\r\n");

    const uint8_t data_match =
        ((load_result == SD_CONFIG_OK) &&
         (loaded_count == 3U) &&
         (loaded[1] == 111) &&
         (loaded[2] == -222) &&
         (loaded[3] == 333)) ? 1U : 0U;

    SerialUSB.print("data_match=");
    SerialUSB.print((data_match != 0U) ? "yes" : "no");
    SerialUSB.print("\r\nresult=");
    SerialUSB.print((data_match != 0U) ? "OK" : "FAILED");
    SerialUSB.print("\r\n");
}
