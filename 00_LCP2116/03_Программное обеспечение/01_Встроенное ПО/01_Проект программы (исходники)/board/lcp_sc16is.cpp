/**
 * @file lcp_sc16is.cpp
 * @brief Карта и одноразовое автоопределение внешних UART SC16IS7xx.
 *
 * Микросхемы являются частью стационарной платы и не рассматриваются как
 * hot-plug устройства. Инициализация SPI/CS и probe поэтому идемпотентны:
 * echo-test, FieldSensor и повторная загрузка serial-конфигурации используют
 * одну сохранённую карту и не повторяют scratchpad-тесты без необходимости.
 * Настройку baud/frame выполняет владелец каждого логического порта.
 */

#include "lcp_sc16is.hpp"
#include "../platform/platform.hpp"

namespace
{
LcpSc16isMap g_sc16is_map =
{
    LCP_SC16IS_LAYOUT_UNKNOWN,
    { LCP_SC16IS_UART1_CS, SC16IS_CHANNEL_A, 0U },
    { LCP_SC16IS_UART2_CS, SC16IS_CHANNEL_A, 0U },
    { LCP_SC16IS_UART3_CS, SC16IS_CHANNEL_A, 0U },
    0U, 0U, 0U, 0U
};

uint8_t g_bus_initialized = 0U;
uint8_t g_probe_complete = 0U;

uint8_t channel_present(uint32_t chip_select, Sc16isChannel channel)
{
    return (sc16is_probe_channel(chip_select, channel) ==
            SC16IS_PROBE_PRESENT) ? 1U : 0U;
}

const char* layout_text(LcpSc16isLayout layout)
{
    switch (layout)
    {
        case LCP_SC16IS_LAYOUT_DUAL_ON_UART2:
            return "dual-channel device on UART2_CS";
        case LCP_SC16IS_LAYOUT_TWO_SINGLE_UART2_UART3:
            return "two single-channel devices on UART2_CS and UART3_CS";
        case LCP_SC16IS_LAYOUT_UNKNOWN:
        default:
            return "unknown or incomplete";
    }
}

void print_present(const char* name, uint8_t present)
{
    SerialUSB.print("  ");
    SerialUSB.print(name);
    SerialUSB.print("=");
    SerialUSB.print((present != 0U) ? "present" : "absent");
    SerialUSB.print("\r\n");
}

void print_assignment(const char* logical_name, const LcpSc16isPort& port)
{
    SerialUSB.print("  ");
    SerialUSB.print(logical_name);
    SerialUSB.print(": present=");
    SerialUSB.print((port.present != 0U) ? "yes" : "no");
    SerialUSB.print(", chip_select=");
    SerialUSB.print(static_cast<unsigned long>(port.chip_select));
    SerialUSB.print(", channel=");
    SerialUSB.print((port.channel == SC16IS_CHANNEL_B) ? "B" : "A");
    SerialUSB.print("\r\n");
}
}

void lcp_sc16is_init_pins(void)
{
    if (g_bus_initialized != 0U)
    {
        return;
    }

    pinMode(LCP_SC16IS_UART1_CS, OUTPUT);
    pinMode(LCP_SC16IS_UART2_CS, OUTPUT);
    pinMode(LCP_SC16IS_UART3_CS, OUTPUT);
    digitalWrite(LCP_SC16IS_UART1_CS, HIGH);
    digitalWrite(LCP_SC16IS_UART2_CS, HIGH);
    digitalWrite(LCP_SC16IS_UART3_CS, HIGH);
    sc16is_init_bus();
    g_bus_initialized = 1U;
}

void lcp_sc16is_probe(void)
{
    if (g_probe_complete != 0U)
    {
        return;
    }

    lcp_sc16is_init_pins();

    g_sc16is_map.layout = LCP_SC16IS_LAYOUT_UNKNOWN;
    g_sc16is_map.uart1_ch_a_present =
        channel_present(LCP_SC16IS_UART1_CS, SC16IS_CHANNEL_A);
    g_sc16is_map.uart2_ch_a_present =
        channel_present(LCP_SC16IS_UART2_CS, SC16IS_CHANNEL_A);

    const uint8_t uart2_dual =
        sc16is_probe_dual_channel(LCP_SC16IS_UART2_CS);
    g_sc16is_map.uart2_ch_b_present = uart2_dual;
    g_sc16is_map.uart3_ch_a_present =
        channel_present(LCP_SC16IS_UART3_CS, SC16IS_CHANNEL_A);

    if (uart2_dual != 0U)
    {
        g_sc16is_map.layout = LCP_SC16IS_LAYOUT_DUAL_ON_UART2;
        g_sc16is_map.hmi =
            { LCP_SC16IS_UART2_CS, SC16IS_CHANNEL_A, 1U };
        g_sc16is_map.s1 =
            { LCP_SC16IS_UART2_CS, SC16IS_CHANNEL_B, 1U };
    }
    else
    {
        if ((g_sc16is_map.uart2_ch_a_present != 0U) &&
            (g_sc16is_map.uart3_ch_a_present != 0U))
        {
            g_sc16is_map.layout =
                LCP_SC16IS_LAYOUT_TWO_SINGLE_UART2_UART3;
        }

        g_sc16is_map.hmi =
            { LCP_SC16IS_UART2_CS,
              SC16IS_CHANNEL_A,
              g_sc16is_map.uart2_ch_a_present };
        g_sc16is_map.s1 =
            { LCP_SC16IS_UART3_CS,
              SC16IS_CHANNEL_A,
              g_sc16is_map.uart3_ch_a_present };
    }

    g_sc16is_map.pc =
        { LCP_SC16IS_UART1_CS,
          SC16IS_CHANNEL_A,
          g_sc16is_map.uart1_ch_a_present };

    g_probe_complete = 1U;
}

const LcpSc16isMap& lcp_sc16is_get_map(void)
{
    lcp_sc16is_probe();
    return g_sc16is_map;
}

void lcp_sc16is_print_probe_report(void)
{
    if (!SerialUSB)
    {
        return;
    }

    lcp_sc16is_probe();

    SerialUSB.print("\r\n[ SC16IS EXTERNAL UARTS ]\r\n");
    SerialUSB.print("probe_complete=");
    SerialUSB.print((g_probe_complete != 0U) ? "yes" : "no");
    SerialUSB.print(", layout=");
    SerialUSB.print(layout_text(g_sc16is_map.layout));
    SerialUSB.print("\r\n\r\n-- Physical channels --\r\n");
    print_present("UART1_CS channel A", g_sc16is_map.uart1_ch_a_present);
    print_present("UART2_CS channel A", g_sc16is_map.uart2_ch_a_present);
    print_present("UART2_CS channel B", g_sc16is_map.uart2_ch_b_present);
    print_present("UART3_CS channel A", g_sc16is_map.uart3_ch_a_present);

    SerialUSB.print("\r\n-- Logical assignment --\r\n");
    print_assignment("PC", g_sc16is_map.pc);
    print_assignment("HMI", g_sc16is_map.hmi);
    print_assignment("S1", g_sc16is_map.s1);
}
