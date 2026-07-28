
/**
 * @file lcp_ethernet.cpp
 * @brief Инициализация GPIO двух Ethernet-контроллеров W5500.
 */

#include "lcp_ethernet.hpp"
#include "../hal/sam3x_device.hpp"
#include "../platform/platform.hpp"

struct LcpEthernetLine
{
    Pio* port;
    uint32_t mask;
    uint32_t peripheral_id;
};

/*
 * ETH1_CS = digital pin 10 = PC29.
 * ETH2_CS = digital pin 48 = PC15.
 */
static const LcpEthernetLine ETH1_CS    = { PIOC, PIO_PC29, ID_PIOC };
static const LcpEthernetLine ETH2_CS    = { PIOC, PIO_PC15, ID_PIOC };
static const LcpEthernetLine ETH1_RESET = { PIOD, PIO_PD8,  ID_PIOD };
static const LcpEthernetLine ETH2_RESET = { PIOC, PIO_PC19, ID_PIOC };
static const LcpEthernetLine ETH1_INT   = { PIOB, PIO_PB25, ID_PIOB };
static const LcpEthernetLine ETH2_INT   = { PIOC, PIO_PC16, ID_PIOC };

static void lcp_ethernet_enable_peripheral_clock(uint32_t peripheral_id)
{
    if (peripheral_id < 32U)
    {
        PMC->PMC_PCER0 = (1UL << peripheral_id);
    }
    else
    {
        PMC->PMC_PCER1 = (1UL << (peripheral_id - 32U));
    }
}

static void lcp_ethernet_configure_output(const LcpEthernetLine& line)
{
    lcp_ethernet_enable_peripheral_clock(line.peripheral_id);

    line.port->PIO_PER = line.mask;
    line.port->PIO_IDR = line.mask;
    line.port->PIO_OER = line.mask;
}

static void lcp_ethernet_configure_input(const LcpEthernetLine& line)
{
    lcp_ethernet_enable_peripheral_clock(line.peripheral_id);

    line.port->PIO_PER = line.mask;
    line.port->PIO_IDR = line.mask;
    line.port->PIO_ODR = line.mask;
    line.port->PIO_PUER = line.mask;
}

static void lcp_ethernet_write_line(const LcpEthernetLine& line, uint8_t value)
{
    if (value != 0U)
    {
        line.port->PIO_SODR = line.mask;
    }
    else
    {
        line.port->PIO_CODR = line.mask;
    }
}

static const LcpEthernetLine& lcp_ethernet_cs_line(LcpEthernetId ethernet_id)
{
    return (ethernet_id == LCP_ETHERNET_2) ? ETH2_CS : ETH1_CS;
}

static const LcpEthernetLine& lcp_ethernet_reset_line(LcpEthernetId ethernet_id)
{
    return (ethernet_id == LCP_ETHERNET_2) ? ETH2_RESET : ETH1_RESET;
}

void lcp_ethernet_init_pins(void)
{
    lcp_ethernet_configure_output(ETH1_CS);
    lcp_ethernet_configure_output(ETH2_CS);
    lcp_ethernet_configure_output(ETH1_RESET);
    lcp_ethernet_configure_output(ETH2_RESET);

    lcp_ethernet_configure_input(ETH1_INT);
    lcp_ethernet_configure_input(ETH2_INT);

    lcp_ethernet_deselect_all();

    lcp_ethernet_write_line(ETH1_RESET, 1U);
    lcp_ethernet_write_line(ETH2_RESET, 1U);
}

void lcp_ethernet_reset(LcpEthernetId ethernet_id)
{
    const LcpEthernetLine& reset = lcp_ethernet_reset_line(ethernet_id);

    lcp_ethernet_write_line(reset, 0U);
    delay(5U);
    lcp_ethernet_write_line(reset, 1U);
    delay(100U);
}

void lcp_ethernet_select(LcpEthernetId ethernet_id)
{
    lcp_ethernet_deselect_all();
    lcp_ethernet_write_line(lcp_ethernet_cs_line(ethernet_id), 0U);
}

void lcp_ethernet_deselect(LcpEthernetId ethernet_id)
{
    lcp_ethernet_write_line(lcp_ethernet_cs_line(ethernet_id), 1U);
}

void lcp_ethernet_deselect_all(void)
{
    lcp_ethernet_write_line(ETH1_CS, 1U);
    lcp_ethernet_write_line(ETH2_CS, 1U);
}

const char* lcp_ethernet_name(LcpEthernetId ethernet_id)
{
    return (ethernet_id == LCP_ETHERNET_2) ? "ETH2" : "ETH1";
}
