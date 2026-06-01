/**
 * @file sam3x_uart.cpp
 * @brief Реализация HAL последовательных портов ATSAM3X8E.
 */

#include "sam3x_uart.hpp"
#include "sam3x_device.hpp"

enum HalUartPeripheralType
{
    HAL_UART_PERIPHERAL_UART,
    HAL_UART_PERIPHERAL_USART
};

struct HalUartPin
{
    Pio* port;
    uint32_t mask;
    uint32_t pio_id;
    uint8_t peripheral_b;
};

struct HalUartDescriptor
{
    HalUartPeripheralType type;
    Uart* uart;
    Usart* usart;
    uint32_t peripheral_id;
    HalUartPin rx;
    HalUartPin tx;
};

static const HalUartDescriptor g_uart_ports[] =
{
    {
        HAL_UART_PERIPHERAL_UART,
        UART,
        0,
        ID_UART,
        { PIOA, (1UL << 8),  ID_PIOA, 0U },
        { PIOA, (1UL << 9),  ID_PIOA, 0U }
    },
    {
        HAL_UART_PERIPHERAL_USART,
        0,
        USART0,
        ID_USART0,
        { PIOA, (1UL << 10), ID_PIOA, 0U },
        { PIOA, (1UL << 11), ID_PIOA, 0U }
    },
    {
        HAL_UART_PERIPHERAL_USART,
        0,
        USART1,
        ID_USART1,
        { PIOA, (1UL << 12), ID_PIOA, 0U },
        { PIOA, (1UL << 13), ID_PIOA, 0U }
    },
    {
        HAL_UART_PERIPHERAL_USART,
        0,
        USART3,
        ID_USART3,
        { PIOD, (1UL << 5),  ID_PIOD, 1U },
        { PIOD, (1UL << 4),  ID_PIOD, 1U }
    }
};

static const HalUartDescriptor* hal_uart_get_descriptor(HalUartId port_id)
{
    const uint32_t index = static_cast<uint32_t>(port_id);
    const uint32_t count = sizeof(g_uart_ports) / sizeof(g_uart_ports[0]);

    if (index >= count)
    {
        return 0;
    }

    return &g_uart_ports[index];
}

static void hal_enable_peripheral_clock(uint32_t peripheral_id)
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

static void hal_uart_configure_pin(const HalUartPin& pin)
{
    hal_enable_peripheral_clock(pin.pio_id);

    pin.port->PIO_IDR = pin.mask;
    pin.port->PIO_PUDR = pin.mask;

    if (pin.peripheral_b != 0U)
    {
        pin.port->PIO_ABSR |= pin.mask;
    }
    else
    {
        pin.port->PIO_ABSR &= ~pin.mask;
    }

    pin.port->PIO_PDR = pin.mask;
}

static uint32_t hal_uart_get_uart_parity(HalUartFrame frame)
{
    switch (frame)
    {
        case HAL_UART_FRAME_8O1:
            return UART_MR_PAR_ODD;

        case HAL_UART_FRAME_8E1:
            return UART_MR_PAR_EVEN;

        case HAL_UART_FRAME_8N1:
        default:
            return UART_MR_PAR_NO;
    }
}

static uint32_t hal_uart_get_usart_parity(HalUartFrame frame)
{
    switch (frame)
    {
        case HAL_UART_FRAME_8O1:
            return US_MR_PAR_ODD;

        case HAL_UART_FRAME_8E1:
            return US_MR_PAR_EVEN;

        case HAL_UART_FRAME_8N1:
        default:
            return US_MR_PAR_NO;
    }
}

static uint32_t hal_uart_get_baud_divisor(uint32_t baudrate)
{
    if (baudrate == 0U)
    {
        return 1U;
    }

    uint32_t divisor = SystemCoreClock / (16U * baudrate);

    if (divisor == 0U)
    {
        divisor = 1U;
    }

    return divisor;
}

void hal_uart_begin(HalUartId port_id, uint32_t baudrate, HalUartFrame frame)
{
    const HalUartDescriptor* descriptor = hal_uart_get_descriptor(port_id);

    if (descriptor == 0)
    {
        return;
    }

    hal_uart_configure_pin(descriptor->rx);
    hal_uart_configure_pin(descriptor->tx);
    hal_enable_peripheral_clock(descriptor->peripheral_id);

    const uint32_t baud_divisor = hal_uart_get_baud_divisor(baudrate);

    if (descriptor->type == HAL_UART_PERIPHERAL_UART)
    {
        Uart* uart = descriptor->uart;

        uart->UART_CR = UART_CR_RSTRX | UART_CR_RSTTX | UART_CR_RXDIS | UART_CR_TXDIS;
        uart->UART_IDR = 0xFFFFFFFFU;
        uart->UART_MR = hal_uart_get_uart_parity(frame);
        uart->UART_BRGR = baud_divisor;
        uart->UART_CR = UART_CR_RXEN | UART_CR_TXEN;
    }
    else
    {
        Usart* usart = descriptor->usart;

        usart->US_CR = US_CR_RSTRX | US_CR_RSTTX | US_CR_RXDIS | US_CR_TXDIS | US_CR_RSTSTA;
        usart->US_IDR = 0xFFFFFFFFU;
        usart->US_MR = US_MR_USART_MODE_NORMAL
                     | US_MR_USCLKS_MCK
                     | US_MR_CHRL_8_BIT
                     | hal_uart_get_usart_parity(frame)
                     | US_MR_NBSTOP_1_BIT
                     | US_MR_CHMODE_NORMAL;
        usart->US_BRGR = baud_divisor;
        usart->US_CR = US_CR_RXEN | US_CR_TXEN;
    }
}

void hal_uart_end(HalUartId port_id)
{
    const HalUartDescriptor* descriptor = hal_uart_get_descriptor(port_id);

    if (descriptor == 0)
    {
        return;
    }

    if (descriptor->type == HAL_UART_PERIPHERAL_UART)
    {
        descriptor->uart->UART_CR = UART_CR_RXDIS | UART_CR_TXDIS;
    }
    else
    {
        descriptor->usart->US_CR = US_CR_RXDIS | US_CR_TXDIS;
    }
}

uint32_t hal_uart_available(HalUartId port_id)
{
    const HalUartDescriptor* descriptor = hal_uart_get_descriptor(port_id);

    if (descriptor == 0)
    {
        return 0U;
    }

    if (descriptor->type == HAL_UART_PERIPHERAL_UART)
    {
        return ((descriptor->uart->UART_SR & UART_SR_RXRDY) != 0U) ? 1U : 0U;
    }

    return ((descriptor->usart->US_CSR & US_CSR_RXRDY) != 0U) ? 1U : 0U;
}

int hal_uart_read(HalUartId port_id)
{
    const HalUartDescriptor* descriptor = hal_uart_get_descriptor(port_id);

    if (descriptor == 0)
    {
        return -1;
    }

    if (descriptor->type == HAL_UART_PERIPHERAL_UART)
    {
        if ((descriptor->uart->UART_SR & UART_SR_RXRDY) == 0U)
        {
            return -1;
        }

        return static_cast<int>(descriptor->uart->UART_RHR & 0xFFU);
    }

    if ((descriptor->usart->US_CSR & US_CSR_RXRDY) == 0U)
    {
        return -1;
    }

    return static_cast<int>(descriptor->usart->US_RHR & 0xFFU);
}

void hal_uart_write(HalUartId port_id, uint8_t value)
{
    const HalUartDescriptor* descriptor = hal_uart_get_descriptor(port_id);

    if (descriptor == 0)
    {
        return;
    }

    if (descriptor->type == HAL_UART_PERIPHERAL_UART)
    {
        while ((descriptor->uart->UART_SR & UART_SR_TXRDY) == 0U)
        {
            __asm__ volatile ("nop");
        }

        descriptor->uart->UART_THR = value;
    }
    else
    {
        while ((descriptor->usart->US_CSR & US_CSR_TXRDY) == 0U)
        {
            __asm__ volatile ("nop");
        }

        descriptor->usart->US_THR = value;
    }
}

void hal_uart_flush(HalUartId port_id)
{
    const HalUartDescriptor* descriptor = hal_uart_get_descriptor(port_id);

    if (descriptor == 0)
    {
        return;
    }

    if (descriptor->type == HAL_UART_PERIPHERAL_UART)
    {
        while ((descriptor->uart->UART_SR & UART_SR_TXEMPTY) == 0U)
        {
            __asm__ volatile ("nop");
        }
    }
    else
    {
        while ((descriptor->usart->US_CSR & US_CSR_TXEMPTY) == 0U)
        {
            __asm__ volatile ("nop");
        }
    }
}
