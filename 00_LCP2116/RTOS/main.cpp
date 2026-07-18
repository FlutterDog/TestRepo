/**
 * @file main.cpp
 * @brief Точка входа диагностической прошивки LCP под управлением FreeRTOS.
 */

#include "sam.h"
#include "app/app.hpp"

extern "C"
{
#include "FreeRTOS.h"
#include "task.h"
}

extern "C" void SystemInit(void);

namespace
{
constexpr uint16_t APP_TASK_STACK_WORDS = 2048U;
constexpr UBaseType_t APP_TASK_PRIORITY = 2U;

void app_task(void *argument)
{
    (void)argument;

    setup();

    for (;;)
    {
        loop();

        /*
         * Существующая диагностическая прошивка построена на неблокирующих
         * poll-функциях. Задержка в один тик сохраняет частый опрос и при этом
         * оставляет процессорное время idle-задаче и последующим RTOS-задачам.
         */
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}
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

int main(void)
{
    SystemInit();

    const BaseType_t task_result = xTaskCreate(app_task,
                                               "LCP",
                                               APP_TASK_STACK_WORDS,
                                               nullptr,
                                               APP_TASK_PRIORITY,
                                               nullptr);

    configASSERT(task_result == pdPASS);

    vTaskStartScheduler();

    /* Планировщик возвращается только при ошибке запуска. */
    for (;;)
    {
    }
}
