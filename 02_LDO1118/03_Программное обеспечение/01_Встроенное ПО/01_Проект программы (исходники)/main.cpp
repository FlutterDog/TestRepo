#include "app/project.hpp"
#include "app/app.hpp"
#include <avr/interrupt.h>

/**
 * @file main.cpp
 * @brief “очка входа приложени€ LDO (AVR).
 *
 * @details
 * ¬ыполн€ет минимальную инициализацию платформы:
 * - запрещает прерывани€ на врем€ настройки базовых модулей;
 * - инициализирует миллисекундный тикер hal::tick_init();
 * - разрешает прерывани€;
 * - передаЄт управление прикладному уровню setup()/loop().
 */

int main(void)
{
    cli();

    hal::tick_init();

    sei();

    setup();

    for (;;) {
        loop();
    }
}