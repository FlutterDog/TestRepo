/**
 * @file app.cpp
 * @brief Диагностическая baseline-прошивка LCP под управлением FreeRTOS.
 *
 * Приложение выполняет heartbeat через PLC_ok, поддерживает USB CDC
 * service console, обслуживает встроенные интерфейсы контроллера и запускает
 * конфигурируемый master внутренней шины X2X.
 */

#include "app.hpp"
#include "../board/lcp_board.hpp"
#include "../board/lcp_rs485.hpp"
#include "../platform/platform.hpp"
#include "../hal/sam3x_watchdog.hpp"
#include "diagnostics/rs485_echo_test.hpp"
#include "diagnostics/sc16is_echo_test.hpp"
#include "diagnostics/ethernet_echo_test.hpp"
#include "diagnostics/diagnostic_console.hpp"
#include "diagnostics/sd_card_test.hpp"
#include "diagnostics/battery_status.hpp"
#include "diagnostics/rtc_status.hpp"
#include "diagnostics/watchdog_status.hpp"
#include "x2x/x2x_service.hpp"

extern "C"
{
#include "FreeRTOS.h"
#include "task.h"
}

namespace
{
constexpr byte PLC_OK_PIN = 40U;
constexpr uint32_t OK_LED_PERIOD_MS = 500U;
constexpr uint16_t LCP_TASK_STACK_WORDS = 2048U;
constexpr UBaseType_t LCP_TASK_PRIORITY = 2U;

TaskHandle_t g_lcp_task_handle = nullptr;

void lcp_task(void *argument)
{
    (void)argument;

    setup();

    for (;;)
    {
        loop();

        /*
         * Все baseline-модули обслуживаются неблокирующими poll-функциями
         * внутри одной задачи. Один тик освобождает процессор для idle-задачи
         * и будущих специализированных задач контроллера.
         */
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}
}

void setup(void)
{
    sam3x_watchdog_boot_recovery_from_watchdog_reset();

    lcp_board_init_gpio();
    lcp_rs485_init_builtin_ports();

    pinMode(PLC_OK_PIN, OUTPUT);
    digitalWrite(PLC_OK_PIN, LOW);

    SerialUSB.begin(115200U, SERIAL_8N1);
    SPI.begin();

    rs485_echo_test_init();
    sc16is_echo_test_init();
    ethernet_echo_test_init();
    sd_card_test_init();
    x2x_service_init();
    battery_status_init();
    rtc_status_init();
    diagnostic_console_init();
    watchdog_status_init();
}

void loop(void)
{
    static uint32_t last_toggle_ms = 0U;
    static uint8_t led_state = LOW;

    const uint32_t now_ms = millis();

    if ((uint32_t)(now_ms - last_toggle_ms) >= OK_LED_PERIOD_MS)
    {
        last_toggle_ms = now_ms;
        led_state = (led_state == LOW) ? HIGH : LOW;
        digitalWrite(PLC_OK_PIN, led_state);
    }

    /*
     * microSD обслуживается раньше X2X, чтобы сервис мог автоматически
     * загрузить X2X.TXT сразу после успешного монтирования файловой системы.
     */
    sd_card_test_poll();
    x2x_service_poll();

    /* UART0 может принадлежать либо X2X master, либо диагностическому echo. */
    rs485_echo_test_set_x2x_enabled(
        (x2x_service_owns_port() == 0U) ? 1U : 0U);
    rs485_echo_test_poll();

    sc16is_echo_test_poll();
    ethernet_echo_test_poll();
    battery_status_poll();
    rtc_status_poll();
    diagnostic_console_poll();
    watchdog_status_poll();
}

void app_rtos_start(void)
{
    const BaseType_t task_result = xTaskCreate(lcp_task,
                                               "LCP",
                                               LCP_TASK_STACK_WORDS,
                                               nullptr,
                                               LCP_TASK_PRIORITY,
                                               &g_lcp_task_handle);

    configASSERT(task_result == pdPASS);

    vTaskStartScheduler();

    /* Планировщик возвращается только при ошибке запуска. */
    taskDISABLE_INTERRUPTS();

    for (;;)
    {
    }
}

uint8_t app_rtos_scheduler_running(void)
{
    return (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) ? 1U : 0U;
}

uint32_t app_rtos_tick_count(void)
{
    return static_cast<uint32_t>(xTaskGetTickCount());
}

uint32_t app_rtos_free_heap_bytes(void)
{
    return static_cast<uint32_t>(xPortGetFreeHeapSize());
}

uint32_t app_rtos_minimum_ever_free_heap_bytes(void)
{
    return static_cast<uint32_t>(xPortGetMinimumEverFreeHeapSize());
}

uint32_t app_rtos_lcp_stack_free_words(void)
{
    if (g_lcp_task_handle == nullptr)
    {
        return 0U;
    }

    return static_cast<uint32_t>(uxTaskGetStackHighWaterMark(g_lcp_task_handle));
}

extern "C" void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();

    for (;;)
    {
    }
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t task,
                                                char *task_name)
{
    (void)task;
    (void)task_name;

    taskDISABLE_INTERRUPTS();

    for (;;)
    {
    }
}
