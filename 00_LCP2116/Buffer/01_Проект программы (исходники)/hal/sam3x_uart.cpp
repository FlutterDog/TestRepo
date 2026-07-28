
/**
 * @file sam3x_uart.cpp
 * @brief Реализация HAL встроенных UART/USART ATSAM3X8E.
 */

#include "sam3x_uart.hpp"
#include "sam3x_device.hpp"

static const uint16_t HAL_UART_BUFFER_SIZE = 256U;

struct HalUartRingBuffer
{
    volatile uint16_t head;
    volatile uint16_t tail;
    uint8_t data[HAL_UART_BUFFER_SIZE];
};

struct HalGpioLine
{
    Pio* port;
    uint32_t mask;
    uint32_t peripheral_id;
};

struct HalUartPort
{
    Uart* uart;
    Usart* usart;
    IRQn_Type irq;
    uint32_t peripheral_id;
    uint8_t is_usart;

    HalUartRingBuffer rx;
    HalUartRingBuffer tx;

    uint8_t initialized;
    uint8_t rs485_enabled;
    uint8_t transmit_level;
    HalGpioLine direction;

    volatile uint32_t error_count;
};

static HalUartPort g_uart_ports[HAL_UART_PORT_COUNT] =
{
    { UART,   0,      UART_IRQn,   ID_UART,   0U, {0U, 0U, {0U}}, {0U, 0U, {0U}}, 0U, 0U, 1U, {0, 0U, 0U}, 0U },
    { 0,      USART0, USART0_IRQn, ID_USART0, 1U, {0U, 0U, {0U}}, {0U, 0U, {0U}}, 0U, 0U, 1U, {0, 0U, 0U}, 0U },
    { 0,      USART1, USART1_IRQn, ID_USART1, 1U, {0U, 0U, {0U}}, {0U, 0U, {0U}}, 0U, 0U, 1U, {0, 0U, 0U}, 0U },
    { 0,      USART3, USART3_IRQn, ID_USART3, 1U, {0U, 0U, {0U}}, {0U, 0U, {0U}}, 0U, 0U, 1U, {0, 0U, 0U}, 0U }
};

static void hal_uart_cpu_relax(void)
{
    __asm__ volatile ("nop");
}

static void hal_uart_enable_peripheral_clock(uint32_t peripheral_id)
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

static void hal_uart_configure_peripheral_pin(Pio* port, uint32_t mask, uint8_t peripheral_b, uint32_t peripheral_id)
{
    hal_uart_enable_peripheral_clock(peripheral_id);

    port->PIO_IDR = mask;
    port->PIO_PUDR = mask;

    if (peripheral_b != 0U)
    {
        port->PIO_ABSR |= mask;
    }
    else
    {
        port->PIO_ABSR &= ~mask;
    }

    port->PIO_PDR = mask;
}

static void hal_uart_configure_output_pin(const HalGpioLine& line)
{
    if (line.port == 0)
    {
        return;
    }

    hal_uart_enable_peripheral_clock(line.peripheral_id);

    line.port->PIO_PER = line.mask;
    line.port->PIO_IDR = line.mask;
    line.port->PIO_OER = line.mask;
}

static HalGpioLine hal_uart_gpio_from_pin(uint32_t pin)
{
    switch (pin)
    {
        case 7U:
            return { PIOC, PIO_PC23, ID_PIOC };

        case 8U:
            return { PIOC, PIO_PC22, ID_PIOC };

        case 9U:
            return { PIOC, PIO_PC21, ID_PIOC };

        case 45U:
            return { PIOC, PIO_PC18, ID_PIOC };

        default:
            return { 0, 0U, 0U };
    }
}

static void hal_uart_gpio_write(const HalGpioLine& line, uint8_t value)
{
    if (line.port == 0)
    {
        return;
    }

    if (value != 0U)
    {
        line.port->PIO_SODR = line.mask;
    }
    else
    {
        line.port->PIO_CODR = line.mask;
    }
}

static void hal_uart_set_receive(HalUartPort& port)
{
    if (port.rs485_enabled == 0U)
    {
        return;
    }

    hal_uart_gpio_write(port.direction, (port.transmit_level != 0U) ? 0U : 1U);
}

static void hal_uart_set_transmit(HalUartPort& port)
{
    if (port.rs485_enabled == 0U)
    {
        return;
    }

    hal_uart_gpio_write(port.direction, port.transmit_level);
}

static void hal_uart_ring_reset(HalUartRingBuffer& buffer)
{
    buffer.head = 0U;
    buffer.tail = 0U;
}

static uint16_t hal_uart_ring_next(uint16_t value)
{
    return static_cast<uint16_t>((value + 1U) % HAL_UART_BUFFER_SIZE);
}

static uint8_t hal_uart_ring_push(HalUartRingBuffer& buffer, uint8_t value)
{
    const uint16_t next = hal_uart_ring_next(buffer.head);

    if (next == buffer.tail)
    {
        return 0U;
    }

    buffer.data[buffer.head] = value;
    buffer.head = next;

    return 1U;
}

static int hal_uart_ring_pop(HalUartRingBuffer& buffer)
{
    if (buffer.head == buffer.tail)
    {
        return -1;
    }

    const uint8_t value = buffer.data[buffer.tail];
    buffer.tail = hal_uart_ring_next(buffer.tail);

    return value;
}

static int hal_uart_ring_peek(const HalUartRingBuffer& buffer)
{
    if (buffer.head == buffer.tail)
    {
        return -1;
    }

    return buffer.data[buffer.tail];
}

static size_t hal_uart_ring_available(const HalUartRingBuffer& buffer)
{
    return static_cast<size_t>((HAL_UART_BUFFER_SIZE + buffer.head - buffer.tail) % HAL_UART_BUFFER_SIZE);
}

static uint32_t hal_uart_mode_from_frame(HalUartFrame frame)
{
    if (frame == HAL_UART_FRAME_8O1)
    {
        return UART_MR_PAR_ODD;
    }

    if (frame == HAL_UART_FRAME_8E1)
    {
        return UART_MR_PAR_EVEN;
    }

    return UART_MR_PAR_NO;
}

static uint32_t hal_usart_mode_from_frame(HalUartFrame frame)
{
    uint32_t mode = US_MR_USART_MODE_NORMAL
                  | US_MR_USCLKS_MCK
                  | US_MR_CHRL_8_BIT
                  | US_MR_NBSTOP_1_BIT
                  | US_MR_CHMODE_NORMAL;

    if (frame == HAL_UART_FRAME_8O1)
    {
        mode |= US_MR_PAR_ODD;
    }
    else if (frame == HAL_UART_FRAME_8E1)
    {
        mode |= US_MR_PAR_EVEN;
    }
    else
    {
        mode |= US_MR_PAR_NO;
    }

    return mode;
}

static void hal_uart_configure_pins(HalUartId port_id)
{
    switch (port_id)
    {
        case HAL_UART_PORT_0:
            hal_uart_configure_peripheral_pin(PIOA, PIO_PA8A_URXD | PIO_PA9A_UTXD, 0U, ID_PIOA);
            break;

        case HAL_UART_PORT_1:
            hal_uart_configure_peripheral_pin(PIOA, PIO_PA10A_RXD0 | PIO_PA11A_TXD0, 0U, ID_PIOA);
            break;

        case HAL_UART_PORT_2:
            hal_uart_configure_peripheral_pin(PIOA, PIO_PA12A_RXD1 | PIO_PA13A_TXD1, 0U, ID_PIOA);
            break;

        case HAL_UART_PORT_3:
            hal_uart_configure_peripheral_pin(PIOD, PIO_PD4B_TXD3 | PIO_PD5B_RXD3, 1U, ID_PIOD);
            break;

        default:
            break;
    }
}

static HalUartPort* hal_uart_get_port(HalUartId port_id)
{
    if (static_cast<uint32_t>(port_id) >= static_cast<uint32_t>(HAL_UART_PORT_COUNT))
    {
        return 0;
    }

    return &g_uart_ports[port_id];
}

static void hal_uart_handle_uart_irq(HalUartPort& port)
{
    const uint32_t status = port.uart->UART_SR;
    const uint32_t interrupt_mask = port.uart->UART_IMR;

    if ((status & UART_SR_RXRDY) != 0U)
    {
        (void)hal_uart_ring_push(port.rx, static_cast<uint8_t>(port.uart->UART_RHR));
    }

    if (((status & UART_SR_TXRDY) != 0U) && ((interrupt_mask & UART_SR_TXRDY) != 0U))
    {
        const int value = hal_uart_ring_pop(port.tx);

        if (value >= 0)
        {
            port.uart->UART_THR = static_cast<uint8_t>(value);
        }
        else
        {
            port.uart->UART_IDR = UART_IDR_TXRDY;
            port.uart->UART_IER = UART_IER_TXEMPTY;
        }
    }

    if (((status & UART_SR_TXEMPTY) != 0U) && ((interrupt_mask & UART_SR_TXEMPTY) != 0U))
    {
        port.uart->UART_IDR = UART_IDR_TXEMPTY;
        hal_uart_set_receive(port);
    }

    if ((status & (UART_SR_OVRE | UART_SR_FRAME)) != 0U)
    {
        ++port.error_count;
        port.uart->UART_CR = UART_CR_RSTSTA;
    }
}

static void hal_uart_handle_usart_irq(HalUartPort& port)
{
    const uint32_t status = port.usart->US_CSR;
    const uint32_t interrupt_mask = port.usart->US_IMR;

    if ((status & US_CSR_RXRDY) != 0U)
    {
        (void)hal_uart_ring_push(port.rx, static_cast<uint8_t>(port.usart->US_RHR));
    }

    if (((status & US_CSR_TXRDY) != 0U) && ((interrupt_mask & US_CSR_TXRDY) != 0U))
    {
        const int value = hal_uart_ring_pop(port.tx);

        if (value >= 0)
        {
            port.usart->US_THR = static_cast<uint8_t>(value);
        }
        else
        {
            port.usart->US_IDR = US_IDR_TXRDY;
            port.usart->US_IER = US_IER_TXEMPTY;
        }
    }

    if (((status & US_CSR_TXEMPTY) != 0U) && ((interrupt_mask & US_CSR_TXEMPTY) != 0U))
    {
        port.usart->US_IDR = US_IDR_TXEMPTY;
        hal_uart_set_receive(port);
    }

    if ((status & (US_CSR_OVRE | US_CSR_FRAME | US_CSR_PARE)) != 0U)
    {
        ++port.error_count;
        port.usart->US_CR = US_CR_RSTSTA;
    }
}

static void hal_uart_irq_handler(HalUartPort& port)
{
    if (port.is_usart == 0U)
    {
        hal_uart_handle_uart_irq(port);
    }
    else
    {
        hal_uart_handle_usart_irq(port);
    }
}

void hal_uart_set_rs485_direction_pin(HalUartId port_id, uint32_t direction_pin, uint8_t transmit_level)
{
    HalUartPort* port = hal_uart_get_port(port_id);

    if (port == 0)
    {
        return;
    }

    port->direction = hal_uart_gpio_from_pin(direction_pin);
    port->transmit_level = (transmit_level != 0U) ? 1U : 0U;
    port->rs485_enabled = (port->direction.port != 0) ? 1U : 0U;

    hal_uart_configure_output_pin(port->direction);
    hal_uart_set_receive(*port);
}

void hal_uart_begin(HalUartId port_id, uint32_t baudrate, HalUartFrame frame)
{
    HalUartPort* port = hal_uart_get_port(port_id);

    if ((port == 0) || (baudrate == 0U))
    {
        return;
    }

    hal_uart_configure_pins(port_id);
    hal_uart_enable_peripheral_clock(port->peripheral_id);

    hal_uart_ring_reset(port->rx);
    hal_uart_ring_reset(port->tx);
    port->error_count = 0U;

    if (port->is_usart == 0U)
    {
        port->uart->UART_PTCR = UART_PTCR_RXTDIS | UART_PTCR_TXTDIS;
        port->uart->UART_CR = UART_CR_RSTRX | UART_CR_RSTTX | UART_CR_RXDIS | UART_CR_TXDIS;
        port->uart->UART_IDR = 0xFFFFFFFFU;
        port->uart->UART_MR = hal_uart_mode_from_frame(frame) | UART_MR_CHMODE_NORMAL;
        port->uart->UART_BRGR = (SystemCoreClock / baudrate) >> 4U;
        port->uart->UART_IER = UART_IER_RXRDY | UART_IER_OVRE | UART_IER_FRAME;
        port->uart->UART_CR = UART_CR_RXEN | UART_CR_TXEN;
    }
    else
    {
        port->usart->US_PTCR = US_PTCR_RXTDIS | US_PTCR_TXTDIS;
        port->usart->US_CR = US_CR_RSTRX | US_CR_RSTTX | US_CR_RXDIS | US_CR_TXDIS;
        port->usart->US_IDR = 0xFFFFFFFFU;
        port->usart->US_MR = hal_usart_mode_from_frame(frame);
        port->usart->US_BRGR = (SystemCoreClock / baudrate) >> 4U;
        port->usart->US_IER = US_IER_RXRDY | US_IER_OVRE | US_IER_FRAME | US_IER_PARE;
        port->usart->US_CR = US_CR_RXEN | US_CR_TXEN;
    }

    NVIC_SetPriority(port->irq, 1U);
    NVIC_EnableIRQ(port->irq);

    port->initialized = 1U;
    hal_uart_set_receive(*port);
}

void hal_uart_end(HalUartId port_id)
{
    HalUartPort* port = hal_uart_get_port(port_id);

    if (port == 0)
    {
        return;
    }

    hal_uart_flush(port_id);
    NVIC_DisableIRQ(port->irq);

    if (port->is_usart == 0U)
    {
        port->uart->UART_IDR = 0xFFFFFFFFU;
        port->uart->UART_CR = UART_CR_RXDIS | UART_CR_TXDIS;
    }
    else
    {
        port->usart->US_IDR = 0xFFFFFFFFU;
        port->usart->US_CR = US_CR_RXDIS | US_CR_TXDIS;
    }

    port->initialized = 0U;
    hal_uart_set_receive(*port);
}

size_t hal_uart_available(HalUartId port_id)
{
    HalUartPort* port = hal_uart_get_port(port_id);

    if (port == 0)
    {
        return 0U;
    }

    return hal_uart_ring_available(port->rx);
}

int hal_uart_read(HalUartId port_id)
{
    HalUartPort* port = hal_uart_get_port(port_id);

    if (port == 0)
    {
        return -1;
    }

    return hal_uart_ring_pop(port->rx);
}

int hal_uart_peek(HalUartId port_id)
{
    HalUartPort* port = hal_uart_get_port(port_id);

    if (port == 0)
    {
        return -1;
    }

    return hal_uart_ring_peek(port->rx);
}

size_t hal_uart_write(HalUartId port_id, uint8_t value)
{
    HalUartPort* port = hal_uart_get_port(port_id);

    if ((port == 0) || (port->initialized == 0U))
    {
        return 0U;
    }

    __disable_irq();

    const uint8_t pushed = hal_uart_ring_push(port->tx, value);

    if (pushed != 0U)
    {
        hal_uart_set_transmit(*port);

        if (port->is_usart == 0U)
        {
            port->uart->UART_IDR = UART_IDR_TXEMPTY;
            port->uart->UART_IER = UART_IER_TXRDY;
        }
        else
        {
            port->usart->US_IDR = US_IDR_TXEMPTY;
            port->usart->US_IER = US_IER_TXRDY;
        }
    }

    __enable_irq();

    return (pushed != 0U) ? 1U : 0U;
}

void hal_uart_flush(HalUartId port_id)
{
    HalUartPort* port = hal_uart_get_port(port_id);

    if (port == 0)
    {
        return;
    }

    for (;;)
    {
        const uint8_t tx_buffer_empty = (port->tx.head == port->tx.tail) ? 1U : 0U;
        uint8_t hardware_empty;

        if (port->is_usart == 0U)
        {
            hardware_empty = ((port->uart->UART_SR & UART_SR_TXEMPTY) != 0U) ? 1U : 0U;
        }
        else
        {
            hardware_empty = ((port->usart->US_CSR & US_CSR_TXEMPTY) != 0U) ? 1U : 0U;
        }

        if ((tx_buffer_empty != 0U) && (hardware_empty != 0U))
        {
            break;
        }

        hal_uart_cpu_relax();
    }
}

uint32_t hal_uart_error_count(HalUartId port_id)
{
    HalUartPort* port = hal_uart_get_port(port_id);

    if (port == 0)
    {
        return 0U;
    }

    return port->error_count;
}

extern "C" void UART_Handler(void)
{
    hal_uart_irq_handler(g_uart_ports[HAL_UART_PORT_0]);
}

extern "C" void USART0_Handler(void)
{
    hal_uart_irq_handler(g_uart_ports[HAL_UART_PORT_1]);
}

extern "C" void USART1_Handler(void)
{
    hal_uart_irq_handler(g_uart_ports[HAL_UART_PORT_2]);
}

extern "C" void USART3_Handler(void)
{
    hal_uart_irq_handler(g_uart_ports[HAL_UART_PORT_3]);
}
