
/**
 * @file ethernet_echo_test.cpp
 * @brief Реализация TCP echo-test 2 W5500.
 */

#include "ethernet_echo_test.hpp"
#include "../../board/lcp_ethernet.hpp"
#include "../../hal/w5500_lite.hpp"
#include "../../platform/platform.hpp"

static const uint16_t ETH1_TCP_ECHO_PORT = 5001U;
static const uint16_t ETH2_TCP_ECHO_PORT = 5002U;

static const W5500NetworkConfig ETH1_CONFIG =
{
    { { 0x02U, 0x4CU, 0x43U, 0x50U, 0x00U, 0x01U } },
    { { 192U, 168U, 1U, 1U } },
    { { 192U, 168U, 1U, 254U } },
    { { 255U, 255U, 255U, 0U } }
};

static const W5500NetworkConfig ETH2_CONFIG =
{
    { { 0x02U, 0x4CU, 0x43U, 0x50U, 0x00U, 0x02U } },
    { { 192U, 168U, 1U, 1U } },
    { { 192U, 168U, 1U, 254U } },
    { { 255U, 255U, 255U, 0U } }
};

static uint8_t g_eth1_ok = 0U;
static uint8_t g_eth2_ok = 0U;
static uint8_t g_ethernet_report_printed = 0U;

static void print_hex8(uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";

    char text[5];
    text[0] = '0';
    text[1] = 'x';
    text[2] = hex[(value >> 4U) & 0x0FU];
    text[3] = hex[value & 0x0FU];
    text[4] = '\0';

    SerialUSB.print(text);
}

static void print_ip(const W5500IpAddress& ip)
{
    SerialUSB.print(static_cast<int>(ip.octet[0]));
    SerialUSB.print(".");
    SerialUSB.print(static_cast<int>(ip.octet[1]));
    SerialUSB.print(".");
    SerialUSB.print(static_cast<int>(ip.octet[2]));
    SerialUSB.print(".");
    SerialUSB.print(static_cast<int>(ip.octet[3]));
}

static void print_interface_report(LcpEthernetId ethernet_id,
                                   const W5500NetworkConfig& config,
                                   uint16_t port,
                                   uint8_t init_ok)
{
    SerialUSB.print(lcp_ethernet_name(ethernet_id));
    SerialUSB.print(": VERSIONR=");
    print_hex8(w5500_lite_version(ethernet_id));
    SerialUSB.print(", PHYCFGR=");
    print_hex8(w5500_lite_phy_status(ethernet_id));
    SerialUSB.print(", link=");
    SerialUSB.print(w5500_lite_link_up(ethernet_id) ? "up" : "down");
    SerialUSB.print(", init=");
    SerialUSB.print(init_ok ? "ok" : "failed");
    SerialUSB.print(", ip=");
    print_ip(config.ip);
    SerialUSB.print(", tcp echo port=");
    SerialUSB.print(static_cast<int>(port));
    SerialUSB.print("\r\n");
}

void ethernet_echo_test_init(void)
{
    lcp_ethernet_init_pins();

    g_eth1_ok = w5500_lite_begin(LCP_ETHERNET_1, ETH1_CONFIG);
    w5500_lite_tcp_server_begin(LCP_ETHERNET_1, ETH1_TCP_ECHO_PORT);

    g_eth2_ok = w5500_lite_begin(LCP_ETHERNET_2, ETH2_CONFIG);
    w5500_lite_tcp_server_begin(LCP_ETHERNET_2, ETH2_TCP_ECHO_PORT);
}

void ethernet_echo_test_poll(void)
{
    if (g_eth1_ok != 0U)
    {
        w5500_lite_tcp_echo_poll(LCP_ETHERNET_1, ETH1_TCP_ECHO_PORT);
    }

    if (g_eth2_ok != 0U)
    {
        w5500_lite_tcp_echo_poll(LCP_ETHERNET_2, ETH2_TCP_ECHO_PORT);
    }
}

void ethernet_echo_test_print_report(void)
{
    if (!SerialUSB)
    {
        return;
    }

    SerialUSB.print("W5500 status\r\n");
    print_interface_report(LCP_ETHERNET_1, ETH1_CONFIG, ETH1_TCP_ECHO_PORT, g_eth1_ok);
    print_interface_report(LCP_ETHERNET_2, ETH2_CONFIG, ETH2_TCP_ECHO_PORT, g_eth2_ok);
}

void ethernet_echo_test_print_report_once(void)
{
    if (g_ethernet_report_printed != 0U)
    {
        return;
    }

    if (!SerialUSB)
    {
        return;
    }

    ethernet_echo_test_print_report();
    g_ethernet_report_printed = 1U;
}
