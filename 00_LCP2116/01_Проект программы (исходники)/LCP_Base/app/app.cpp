
/**
 * @file app.cpp
 * @brief Минимальная прикладная реализация для проверки каркаса проекта.
 *
 * setup() выполняет инициализацию дискретных линий платы, встроенных
 * последовательных портов, USB CDC и SPI-шины. loop() сохраняет
 * непрерывное выполнение основного цикла.
 */

#include "app.hpp"
#include "../board/lcp_board.hpp"
#include "../platform/platform.hpp"

void setup(void)
{
    lcp_board_init_gpio();

    Serial.begin(9600U, SERIAL_8N1);
    Serial1.begin(9600U, SERIAL_8N1);
    Serial2.begin(9600U, SERIAL_8N1);
    Serial3.begin(9600U, SERIAL_8N1);

    SerialUSB.begin(115200U, SERIAL_8N1);

    SPI.begin();
}

void loop(void)
{
    delay(1U);
}
