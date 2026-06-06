
/**
 * @file app.cpp
 * @brief Минимальная прикладная реализация для проверки запуска прошивки и USB CDC.
 *
 * setup() выполняет инициализацию дискретных линий платы, встроенных
 * последовательных портов, USB CDC, SPI-шины и диагностического светодиода.
 * loop() выполняет неблокирующее мигание светодиода OK и отправляет
 * диагностическое сообщение в открытый USB CDC-порт.
 */

#include "app.hpp"
#include "../board/lcp_board.hpp"
#include "../platform/platform.hpp"

static const byte PLC_ok = 40U;

static const uint32_t OK_LED_PERIOD_MS = 500U;
static const uint32_t USB_TEST_MESSAGE_PERIOD_MS = 1000U;

void setup(void)
{
    lcp_board_init_gpio();

    pinMode(PLC_ok, OUTPUT);
    digitalWrite(PLC_ok, LOW);

    Serial.begin(9600U, SERIAL_8N1);
    Serial1.begin(9600U, SERIAL_8N1);
    Serial2.begin(9600U, SERIAL_8N1);
    Serial3.begin(9600U, SERIAL_8N1);

    SerialUSB.begin(115200U, SERIAL_8N1);

    SPI.begin();
}

void loop(void)
{
    static uint32_t last_toggle_ms = 0U;
    static uint32_t last_usb_message_ms = 0U;
    static uint8_t led_state = LOW;

    const uint32_t now_ms = millis();

    if ((uint32_t)(now_ms - last_toggle_ms) >= OK_LED_PERIOD_MS)
    {
        last_toggle_ms = now_ms;
        led_state = (led_state == LOW) ? HIGH : LOW;
        digitalWrite(PLC_ok, led_state);
    }

    if ((uint32_t)(now_ms - last_usb_message_ms) >= USB_TEST_MESSAGE_PERIOD_MS)
    {
        last_usb_message_ms = now_ms;

        if (SerialUSB)
        {
            SerialUSB.write((const uint8_t*)"LCP USB CDC OK\r\n", 16U);
        }
    }
}
