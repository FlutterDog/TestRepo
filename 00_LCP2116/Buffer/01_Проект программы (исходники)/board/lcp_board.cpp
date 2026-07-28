/**
 * @file lcp_board.cpp
 * @brief Инициализация дискретных линий платы LCP Basic.
 */

#include "lcp_board.hpp"

void lcp_board_init_gpio(void)
{
    pinMode(LCP_PIN_WIZ_INT, INPUT);

    pinMode(LCP_PIN_UART1_CS, OUTPUT);
    pinMode(LCP_PIN_UART2_CS, OUTPUT);
    pinMode(LCP_PIN_UART3_CS, OUTPUT);

    pinMode(LCP_PIN_MICROSD_CS, OUTPUT);
    pinMode(LCP_PIN_ETH1_CS, OUTPUT);
    pinMode(LCP_PIN_ETH2_CS, OUTPUT);

    pinMode(LCP_PIN_X2X_LED, OUTPUT);
    pinMode(LCP_PIN_PLC_OK, OUTPUT);
    pinMode(LCP_PIN_SD_OK, OUTPUT);
    pinMode(LCP_PIN_X2X_TRANSMIT, OUTPUT);

    pinMode(LCP_PIN_A0, OUTPUT);
    pinMode(LCP_PIN_A7, OUTPUT);

    digitalWrite(LCP_PIN_A7, LOW);
    digitalWrite(LCP_PIN_A0, HIGH);

    digitalWrite(LCP_PIN_X2X_LED, LOW);

    digitalWrite(LCP_PIN_UART1_CS, HIGH);
    digitalWrite(LCP_PIN_UART2_CS, HIGH);
    digitalWrite(LCP_PIN_UART3_CS, HIGH);

    digitalWrite(LCP_PIN_MICROSD_CS, HIGH);
    digitalWrite(LCP_PIN_ETH1_CS, HIGH);
    digitalWrite(LCP_PIN_ETH2_CS, HIGH);

    digitalWrite(LCP_PIN_X2X_TRANSMIT, LOW);

    pinMode(LCP_PIN_RS485_DE_0, OUTPUT);
    pinMode(LCP_PIN_RS485_DE_1, OUTPUT);
    pinMode(LCP_PIN_RS485_DE_2, OUTPUT);
    pinMode(LCP_PIN_RS485_DE_3, OUTPUT);

    digitalWrite(LCP_PIN_RS485_DE_0, LCP_RS485_RECEIVE);
    digitalWrite(LCP_PIN_RS485_DE_1, LCP_RS485_RECEIVE);
    digitalWrite(LCP_PIN_RS485_DE_2, LCP_RS485_RECEIVE);
    digitalWrite(LCP_PIN_RS485_DE_3, LCP_RS485_RECEIVE);

    delay(100U);
    digitalWrite(LCP_PIN_RS485_DE_0, LCP_RS485_TRANSMIT);

    delay(100U);
    digitalWrite(LCP_PIN_RS485_DE_1, LCP_RS485_TRANSMIT);

    delay(100U);
    digitalWrite(LCP_PIN_RS485_DE_2, LCP_RS485_TRANSMIT);

    delay(100U);
    digitalWrite(LCP_PIN_RS485_DE_3, LCP_RS485_TRANSMIT);

    delay(100U);
    digitalWrite(LCP_PIN_RS485_DE_0, LCP_RS485_RECEIVE);
    digitalWrite(LCP_PIN_RS485_DE_1, LCP_RS485_RECEIVE);
    digitalWrite(LCP_PIN_RS485_DE_2, LCP_RS485_RECEIVE);
    digitalWrite(LCP_PIN_RS485_DE_3, LCP_RS485_RECEIVE);
}
