/**
 * @file lcp_sc16is.cpp
 * @brief Карта и автоопределение внешних UART SC16IS7xx.
 */

#include "lcp_sc16is.hpp"
#include "../platform/platform.hpp"

static LcpSc16isMap g_sc16is_map =
{
    LCP_SC16IS_LAYOUT_UNKNOWN,
    { LCP_SC16IS_UART1_CS, SC16IS_CHANNEL_A, 0U },
    { LCP_SC16IS_UART2_CS, SC16IS_CHANNEL_A, 0U },
    { LCP_SC16IS_UART3_CS, SC16IS_CHANNEL_A, 0U },
    0U, 0U, 0U, 0U
};

static uint8_t lcp_sc16is_present(uint32_t chip_select,
                                   Sc16isChannel channel)
{
    return (sc16is_probe_channel(chip_select, channel) ==
            SC16IS_PROBE_PRESENT) ? 1U : 0U;
}

static void lcp_sc16is_print_present(const char* name, uint8_t present)
{
    SerialUSB.print(name);
    SerialUSB.print(present ? ": present\r\n" : ": absent\r\n");
}

void lcp_sc16is_init_pins(void)
{
    pinMode(LCP_SC16IS_UART1_CS, OUTPUT);
    pinMode(LCP_SC16IS_UART2_CS, OUTPUT);
    pinMode(LCP_SC16IS_UART3_CS, OUTPUT);
    digitalWrite(LCP_SC16IS_UART1_CS, HIGH);
    digitalWrite(LCP_SC16IS_UART2_CS, HIGH);
    digitalWrite(LCP_SC16IS_UART3_CS, HIGH);
    sc16is_init_bus();
}

void lcp_sc16is_probe(void)
{
    g_sc16is_map.layout = LCP_SC16IS_LAYOUT_UNKNOWN;
    g_sc16is_map.uart1_ch_a_present =
        lcp_sc16is_present(LCP_SC16IS_UART1_CS, SC16IS_CHANNEL_A);
    g_sc16is_map.uart2_ch_a_present =
        lcp_sc16is_present(LCP_SC16IS_UART2_CS, SC16IS_CHANNEL_A);

    const uint8_t uart2_dual =
        sc16is_probe_dual_channel(LCP_SC16IS_UART2_CS);
    g_sc16is_map.uart2_ch_b_present = uart2_dual;

    if (uart2_dual != 0U)
    {
        g_sc16is_map.layout = LCP_SC16IS_LAYOUT_DUAL_ON_UART2;
        g_sc16is_map.uart3_ch_a_present =
            lcp_sc16is_present(LCP_SC16IS_UART3_CS, SC16IS_CHANNEL_A);
        g_sc16is_map.hmi =
            { LCP_SC16IS_UART2_CS, SC16IS_CHANNEL_A, 1U };
        g_sc16is_map.s1 =
            { LCP_SC16IS_UART2_CS, SC16IS_CHANNEL_B, 1U };
    }
    else
    {
        g_sc16is_map.uart3_ch_a_present =
            lcp_sc16is_present(LCP_SC16IS_UART3_CS, SC16IS_CHANNEL_A);

        if ((g_sc16is_map.uart2_ch_a_present != 0U) &&
            (g_sc16is_map.uart3_ch_a_present != 0U))
        {
            g_sc16is_map.layout =
                LCP_SC16IS_LAYOUT_TWO_SINGLE_UART2_UART3;
        }

        g_sc16is_map.hmi =
            { LCP_SC16IS_UART2_CS, SC16IS_CHANNEL_A,
              g_sc16is_map.uart2_ch_a_present };
        g_sc16is_map.s1 =
            { LCP_SC16IS_UART3_CS, SC16IS_CHANNEL_A,
              g_sc16is_map.uart3_ch_a_present };
    }

    g_sc16is_map.pc =
        { LCP_SC16IS_UART1_CS, SC16IS_CHANNEL_A,
          g_sc16is_map.uart1_ch_a_present };
}

const LcpSc16isMap& lcp_sc16is_get_map(void)
{
    return g_sc16is_map;
}

void lcp_sc16is_print_probe_report(void)
{
    if (!SerialUSB)
    {
        return;
    }

    SerialUSB.print("SC16IS probe started\r\n");
    lcp_sc16is_print_present("UART1_CS CH_A", g_sc16is_map.uart1_ch_a_present);
    lcp_sc16is_print_present("UART2_CS CH_A", g_sc16is_map.uart2_ch_a_present);
    lcp_sc16is_print_present("UART2_CS CH_B", g_sc16is_map.uart2_ch_b_present);
    lcp_sc16is_print_present("UART3_CS CH_A", g_sc16is_map.uart3_ch_a_present);

    if (g_sc16is_map.layout == LCP_SC16IS_LAYOUT_DUAL_ON_UART2)
    {
        SerialUSB.print("Detected SC16IS layout: dual-channel at UART2_CS\r\n");
        SerialUSB.print("HMI: UART2_CS CH_A\r\n");
        SerialUSB.print("S1: UART2_CS CH_B\r\n");
    }
    else if (g_sc16is_map.layout ==
             LCP_SC16IS_LAYOUT_TWO_SINGLE_UART2_UART3)
    {
        SerialUSB.print("Detected SC16IS layout: two single-channel devices\r\n");
        SerialUSB.print("HMI: UART2_CS CH_A\r\n");
        SerialUSB.print("S1: UART3_CS CH_A\r\n");
    }
    else
    {
        SerialUSB.print("Detected SC16IS layout: unknown\r\n");
    }

    if (g_sc16is_map.pc.present != 0U)
    {
        SerialUSB.print("PC: UART1_CS CH_A\r\n");
    }

    SerialUSB.print("SC16IS probe done\r\n");
}

void lcp_sc16is_begin_detected_ports(uint32_t baudrate)
{
    if (g_sc16is_map.pc.present != 0U)
    {
        sc16is_begin(g_sc16is_map.pc.chip_select,
                     g_sc16is_map.pc.channel,
                     baudrate,
                     HAL_UART_FRAME_8N1);
    }

    if (g_sc16is_map.hmi.present != 0U)
    {
        sc16is_begin(g_sc16is_map.hmi.chip_select,
                     g_sc16is_map.hmi.channel,
                     baudrate,
                     HAL_UART_FRAME_8N1);
    }

    if (g_sc16is_map.s1.present != 0U)
    {
        sc16is_begin(g_sc16is_map.s1.chip_select,
                     g_sc16is_map.s1.channel,
                     baudrate,
                     HAL_UART_FRAME_8N1);
    }
}
