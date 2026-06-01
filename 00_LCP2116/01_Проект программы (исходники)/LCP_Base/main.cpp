/**
 * @file main.cpp
 * @brief Точка входа прикладной программы LCP Basic.
 *
 * Инициализация ядра и тактирования выполняется startup-кодом проекта.
 * main() запускает системный таймер, выполняет setup() и далее непрерывно
 * вызывает loop().
 */

#include "app/app.hpp"
#include "hal/sam3x_tick.hpp"

int main(void)
{
    hal_tick_init();

    setup();

    for (;;)
    {
        loop();
    }
}
