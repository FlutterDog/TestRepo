
/**
 * @file sam3x_usb_device.cpp
 * @brief Реализация USB CDC device для ATSAM3X8E.
 */

#include "sam3x_usb_device.hpp"
#include "sam3x_reset.hpp"
#include "sam3x_device.hpp"

#include <string.h>

#ifndef USBCON
#define USBCON 1
#endif

#define HAL_USB_CDC_PRODUCT_NAME "LCP Basic"
#define HAL_USB_CDC_MANUFACTURER_NAME "IGAS"

static const uint16_t HAL_USB_VID = 0x2341U;
static const uint16_t HAL_USB_PID = 0x003EU;

static const uint8_t EP0 = 0U;
static const uint8_t CDC_ENDPOINT_ACM = 1U;
static const uint8_t CDC_ENDPOINT_OUT = 2U;
static const uint8_t CDC_ENDPOINT_IN = 3U;

static const uint8_t CDC_ACM_INTERFACE = 0U;
static const uint8_t CDC_DATA_INTERFACE = 1U;

static const uint8_t CDC_RX = CDC_ENDPOINT_OUT;
static const uint8_t CDC_TX = CDC_ENDPOINT_IN;

static const uint32_t USB_ENDPOINT_COUNT = 4U;
static const uint32_t EP0_SIZE = 64U;
static const uint32_t EPX_SIZE = 512U;

#ifndef UOTGHS_RAM_ADDR
#define UOTGHS_RAM_ADDR 0x20100000U
#endif

static const uint32_t UOTGHS_ENDPOINT_FIFO_STRIDE = 0x8000U;

static const uint32_t USB_WAIT_TIMEOUT_ITERATIONS = 100000U;
static const uint32_t USB_BOOTLOADER_DELAY_MS = 250U;
static const uint32_t CDC_RX_BUFFER_SIZE = 512U;

static const uint8_t REQUEST_HOSTTODEVICE = 0x00U;
static const uint8_t REQUEST_DEVICETOHOST = 0x80U;
static const uint8_t REQUEST_STANDARD = 0x00U;
static const uint8_t REQUEST_CLASS = 0x20U;
static const uint8_t REQUEST_TYPE = 0x60U;
static const uint8_t REQUEST_DEVICE = 0x00U;
static const uint8_t REQUEST_INTERFACE = 0x01U;
static const uint8_t REQUEST_RECIPIENT = 0x1FU;

static const uint8_t REQUEST_DEVICETOHOST_CLASS_INTERFACE = REQUEST_DEVICETOHOST | REQUEST_CLASS | REQUEST_INTERFACE;
static const uint8_t REQUEST_HOSTTODEVICE_CLASS_INTERFACE = REQUEST_HOSTTODEVICE | REQUEST_CLASS | REQUEST_INTERFACE;

static const uint8_t GET_STATUS = 0U;
static const uint8_t CLEAR_FEATURE = 1U;
static const uint8_t SET_FEATURE = 3U;
static const uint8_t SET_ADDRESS = 5U;
static const uint8_t GET_DESCRIPTOR = 6U;
static const uint8_t SET_DESCRIPTOR = 7U;
static const uint8_t GET_CONFIGURATION = 8U;
static const uint8_t SET_CONFIGURATION = 9U;
static const uint8_t GET_INTERFACE = 10U;
static const uint8_t SET_INTERFACE = 11U;

static const uint8_t CDC_SET_LINE_CODING = 0x20U;
static const uint8_t CDC_GET_LINE_CODING = 0x21U;
static const uint8_t CDC_SET_CONTROL_LINE_STATE = 0x22U;
static const uint8_t CDC_SEND_BREAK = 0x23U;

static const uint8_t USB_DEVICE_DESCRIPTOR_TYPE = 1U;
static const uint8_t USB_CONFIGURATION_DESCRIPTOR_TYPE = 2U;
static const uint8_t USB_STRING_DESCRIPTOR_TYPE = 3U;
static const uint8_t USB_DEVICE_QUALIFIER = 6U;
static const uint8_t USB_OTHER_SPEED_CONFIGURATION = 7U;

static const uint8_t USB_ENDPOINT_TYPE_BULK = 0x02U;
static const uint8_t USB_ENDPOINT_TYPE_INTERRUPT = 0x03U;

static const uint8_t CDC_COMMUNICATION_INTERFACE_CLASS = 0x02U;
static const uint8_t CDC_ABSTRACT_CONTROL_MODEL = 0x02U;
static const uint8_t CDC_DATA_INTERFACE_CLASS = 0x0AU;
static const uint8_t CDC_CS_INTERFACE = 0x24U;
static const uint8_t CDC_HEADER = 0x00U;
static const uint8_t CDC_CALL_MANAGEMENT = 0x01U;
static const uint8_t CDC_ABSTRACT_CONTROL_MANAGEMENT = 0x02U;
static const uint8_t CDC_UNION = 0x06U;

static const uint8_t IMANUFACTURER = 1U;
static const uint8_t IPRODUCT = 2U;
static const uint8_t ISERIAL = 3U;

static const uint8_t CDC_LINESTATE_DTR = 0x01U;
static const uint8_t CDC_LINESTATE_RTS = 0x02U;

#pragma pack(push, 1)
struct UsbSetup
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint8_t wValueL;
    uint8_t wValueH;
    uint16_t wIndex;
    uint16_t wLength;
};

struct DeviceDescriptor
{
    uint8_t len;
    uint8_t dtype;
    uint16_t usbVersion;
    uint8_t deviceClass;
    uint8_t deviceSubClass;
    uint8_t deviceProtocol;
    uint8_t packetSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t deviceVersion;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
};

struct QualifierDescriptor
{
    uint8_t len;
    uint8_t dtype;
    uint16_t usbVersion;
    uint8_t deviceClass;
    uint8_t deviceSubClass;
    uint8_t deviceProtocol;
    uint8_t packetSize0;
    uint8_t bNumConfigurations;
};

struct ConfigDescriptor
{
    uint8_t len;
    uint8_t dtype;
    uint16_t clen;
    uint8_t numInterfaces;
    uint8_t config;
    uint8_t iconfig;
    uint8_t attributes;
    uint8_t maxPower;
};

struct InterfaceAssociationDescriptor
{
    uint8_t len;
    uint8_t dtype;
    uint8_t firstInterface;
    uint8_t interfaceCount;
    uint8_t functionClass;
    uint8_t functionSubClass;
    uint8_t functionProtocol;
    uint8_t iInterface;
};

struct InterfaceDescriptor
{
    uint8_t len;
    uint8_t dtype;
    uint8_t number;
    uint8_t alternate;
    uint8_t numEndpoints;
    uint8_t interfaceClass;
    uint8_t interfaceSubClass;
    uint8_t protocol;
    uint8_t iInterface;
};

struct EndpointDescriptor
{
    uint8_t len;
    uint8_t dtype;
    uint8_t addr;
    uint8_t attr;
    uint16_t packetSize;
    uint8_t interval;
};

struct CdcCsInterfaceDescriptor5
{
    uint8_t len;
    uint8_t dtype;
    uint8_t subtype;
    uint8_t d0;
    uint8_t d1;
};

struct CdcCsInterfaceDescriptor4
{
    uint8_t len;
    uint8_t dtype;
    uint8_t subtype;
    uint8_t d0;
};

struct CdcDescriptor
{
    InterfaceAssociationDescriptor iad;
    InterfaceDescriptor controlInterface;
    CdcCsInterfaceDescriptor5 header;
    CdcCsInterfaceDescriptor5 callManagement;
    CdcCsInterfaceDescriptor4 controlManagement;
    CdcCsInterfaceDescriptor5 functionalDescriptor;
    EndpointDescriptor acmEndpoint;
    InterfaceDescriptor dataInterface;
    EndpointDescriptor dataInEndpoint;
    EndpointDescriptor dataOutEndpoint;
};

struct LineInfo
{
    uint32_t dwDTERate;
    uint8_t bCharFormat;
    uint8_t bParityType;
    uint8_t bDataBits;
    uint8_t lineState;
};
#pragma pack(pop)

static const DeviceDescriptor g_device_descriptor =
{
    18U, 1U, 0x0200U, 0x00U, 0x00U, 0x00U, 64U,
    HAL_USB_VID, HAL_USB_PID, 0x0100U,
    IMANUFACTURER, IPRODUCT, ISERIAL, 1U
};

static const DeviceDescriptor g_device_descriptor_iad =
{
    18U, 1U, 0x0200U, 0xEFU, 0x02U, 0x01U, 64U,
    HAL_USB_VID, HAL_USB_PID, 0x0100U,
    IMANUFACTURER, IPRODUCT, ISERIAL, 1U
};

static const QualifierDescriptor g_device_qualifier =
{
    10U, USB_DEVICE_QUALIFIER, 0x0200U, 0x00U, 0x00U, 0x00U, 64U, 1U
};

static const CdcDescriptor g_cdc_descriptor =
{
    { 8U, 11U, 0U, 2U, CDC_COMMUNICATION_INTERFACE_CLASS, CDC_ABSTRACT_CONTROL_MODEL, 1U, 0U },
    { 9U, 4U, CDC_ACM_INTERFACE, 0U, 1U, CDC_COMMUNICATION_INTERFACE_CLASS, CDC_ABSTRACT_CONTROL_MODEL, 0U, 0U },
    { 5U, CDC_CS_INTERFACE, CDC_HEADER, 0x10U, 0x01U },
    { 5U, CDC_CS_INTERFACE, CDC_CALL_MANAGEMENT, 1U, 1U },
    { 4U, CDC_CS_INTERFACE, CDC_ABSTRACT_CONTROL_MANAGEMENT, 6U },
    { 5U, CDC_CS_INTERFACE, CDC_UNION, CDC_ACM_INTERFACE, CDC_DATA_INTERFACE },
    { 7U, 5U, static_cast<uint8_t>(0x80U | CDC_ENDPOINT_ACM), USB_ENDPOINT_TYPE_INTERRUPT, 0x0010U, 0x10U },
    { 9U, 4U, CDC_DATA_INTERFACE, 0U, 2U, CDC_DATA_INTERFACE_CLASS, 0U, 0U, 0U },
    { 7U, 5U, static_cast<uint8_t>(0x80U | CDC_ENDPOINT_IN), USB_ENDPOINT_TYPE_BULK, 0x0200U, 0U },
    { 7U, 5U, CDC_ENDPOINT_OUT, USB_ENDPOINT_TYPE_BULK, 0x0200U, 0U }
};

static const CdcDescriptor g_cdc_other_speed_descriptor =
{
    { 8U, 11U, 0U, 2U, CDC_COMMUNICATION_INTERFACE_CLASS, CDC_ABSTRACT_CONTROL_MODEL, 1U, 0U },
    { 9U, 4U, CDC_ACM_INTERFACE, 0U, 1U, CDC_COMMUNICATION_INTERFACE_CLASS, CDC_ABSTRACT_CONTROL_MODEL, 0U, 0U },
    { 5U, CDC_CS_INTERFACE, CDC_HEADER, 0x10U, 0x01U },
    { 5U, CDC_CS_INTERFACE, CDC_CALL_MANAGEMENT, 1U, 1U },
    { 4U, CDC_CS_INTERFACE, CDC_ABSTRACT_CONTROL_MANAGEMENT, 6U },
    { 5U, CDC_CS_INTERFACE, CDC_UNION, CDC_ACM_INTERFACE, CDC_DATA_INTERFACE },
    { 7U, 5U, static_cast<uint8_t>(0x80U | CDC_ENDPOINT_ACM), USB_ENDPOINT_TYPE_INTERRUPT, 0x0010U, 0x10U },
    { 9U, 4U, CDC_DATA_INTERFACE, 0U, 2U, CDC_DATA_INTERFACE_CLASS, 0U, 0U, 0U },
    { 7U, 5U, static_cast<uint8_t>(0x80U | CDC_ENDPOINT_IN), USB_ENDPOINT_TYPE_BULK, 0x0040U, 0U },
    { 7U, 5U, CDC_ENDPOINT_OUT, USB_ENDPOINT_TYPE_BULK, 0x0040U, 0U }
};

static volatile uint32_t g_usb_configuration = 0U;
static volatile uint32_t g_usb_initialized = 0U;
static volatile uint32_t g_usb_set_interface = 0U;
static volatile uint32_t g_usb_error = 0U;
static volatile uint32_t g_usb_composite = 0U;

static volatile LineInfo g_line_info =
{
    57600U, 0U, 0U, 8U, 0U
};

static volatile int32_t g_break_value = -1;

static volatile uint32_t g_send_fifo_ptr[USB_ENDPOINT_COUNT] = { 0U };
static volatile uint32_t g_recv_fifo_ptr[USB_ENDPOINT_COUNT] = { 0U };

static uint8_t g_rx_buffer[CDC_RX_BUFFER_SIZE];
static volatile uint32_t g_rx_head = 0U;
static volatile uint32_t g_rx_tail = 0U;

static inline void usb_cpu_relax(void)
{
    __asm__ volatile ("nop");
}

static uint32_t usb_ep_max_packet(uint32_t ep)
{
    return (ep == EP0) ? EP0_SIZE : EPX_SIZE;
}

static uint32_t usb_wait_for_flag(volatile uint32_t* reg, uint32_t mask)
{
    for (uint32_t i = 0U; i < USB_WAIT_TIMEOUT_ITERATIONS; ++i)
    {
        if ((*reg & mask) != 0U)
        {
            return 1U;
        }

        usb_cpu_relax();
    }

    g_usb_error = 1U;
    return 0U;
}

static void usb_enable_peripheral_clock(uint32_t peripheral_id)
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

static void usb_enable_clocks(void)
{
    usb_enable_peripheral_clock(ID_UOTGHS);

#ifdef CKGR_UCKR_UPLLEN
    PMC->CKGR_UCKR = CKGR_UCKR_UPLLCOUNT(3U) | CKGR_UCKR_UPLLEN;
    while ((PMC->PMC_SR & PMC_SR_LOCKU) == 0U)
    {
    }
#endif

#ifdef PMC_USB_USBS
    PMC->PMC_USB = PMC_USB_USBS | PMC_USB_USBDIV(0U);
#endif

#ifdef PMC_SCER_UDP
    PMC->PMC_SCER = PMC_SCER_UDP;
#endif
}

static void usb_force_device_mode(void)
{
#ifdef UOTGHS_CTRL_UIMOD
    UOTGHS->UOTGHS_CTRL |= UOTGHS_CTRL_UIMOD;
#endif

#ifdef UOTGHS_CTRL_UIDE
    UOTGHS->UOTGHS_CTRL &= ~UOTGHS_CTRL_UIDE;
#endif
}

static void usb_enable_pad_and_clock(void)
{
#ifdef UOTGHS_CTRL_OTGPADE
    UOTGHS->UOTGHS_CTRL |= UOTGHS_CTRL_OTGPADE;
#endif

#ifdef UOTGHS_CTRL_USBE
    UOTGHS->UOTGHS_CTRL |= UOTGHS_CTRL_USBE;
#endif

#ifdef UOTGHS_CTRL_FRZCLK
    UOTGHS->UOTGHS_CTRL &= ~UOTGHS_CTRL_FRZCLK;
#endif

#ifdef UOTGHS_SR_CLKUSABLE
    (void)usb_wait_for_flag(&UOTGHS->UOTGHS_SR, UOTGHS_SR_CLKUSABLE);
#endif
}

static void usb_freeze_clock(void)
{
#ifdef UOTGHS_CTRL_FRZCLK
    UOTGHS->UOTGHS_CTRL |= UOTGHS_CTRL_FRZCLK;
#endif
}

static uint32_t usb_endpoint_fifo_byte_count(uint32_t ep)
{
    return ((UOTGHS->UOTGHS_DEVEPTISR[ep] & UOTGHS_DEVEPTISR_BYCT_Msk) >> UOTGHS_DEVEPTISR_BYCT_Pos);
}

static void usb_release_rx(uint32_t ep)
{
    UOTGHS->UOTGHS_DEVEPTICR[ep] = UOTGHS_DEVEPTICR_RXOUTIC;
#ifdef UOTGHS_DEVEPTIDR_FIFOCONC
    UOTGHS->UOTGHS_DEVEPTIDR[ep] = UOTGHS_DEVEPTIDR_FIFOCONC;
#endif
    g_recv_fifo_ptr[ep] = 0U;
}

static void usb_release_tx(uint32_t ep)
{
    UOTGHS->UOTGHS_DEVEPTICR[ep] = UOTGHS_DEVEPTICR_TXINIC;
#ifdef UOTGHS_DEVEPTIDR_FIFOCONC
    UOTGHS->UOTGHS_DEVEPTIDR[ep] = UOTGHS_DEVEPTIDR_FIFOCONC;
#endif
    g_send_fifo_ptr[ep] = 0U;
}

static volatile uint8_t* usb_endpoint_fifo(uint32_t ep)
{
    return reinterpret_cast<volatile uint8_t*>(UOTGHS_RAM_ADDR + (ep * UOTGHS_ENDPOINT_FIFO_STRIDE));
}

static uint8_t usb_recv8(uint32_t ep)
{
    volatile uint8_t* fifo = usb_endpoint_fifo(ep);
    const uint8_t value = fifo[g_recv_fifo_ptr[ep]];
    ++g_recv_fifo_ptr[ep];
    return value;
}

static void usb_recv(uint32_t ep, uint8_t* data, uint32_t length)
{
    volatile uint8_t* fifo = usb_endpoint_fifo(ep);

    for (uint32_t i = 0U; i < length; ++i)
    {
        data[i] = fifo[g_recv_fifo_ptr[ep] + i];
    }

    g_recv_fifo_ptr[ep] += length;
}

static void usb_send8(uint32_t ep, uint8_t data)
{
    volatile uint8_t* fifo = usb_endpoint_fifo(ep);
    fifo[g_send_fifo_ptr[ep]] = data;
    ++g_send_fifo_ptr[ep];
}

static uint32_t usb_send(uint32_t ep, const void* data, uint32_t length)
{
    const uint8_t* source = static_cast<const uint8_t*>(data);
    volatile uint8_t* fifo = usb_endpoint_fifo(ep);
    const uint32_t max_packet = usb_ep_max_packet(ep);
    uint32_t sent = 0U;

    while (sent < length)
    {
        if (usb_wait_for_flag(&UOTGHS->UOTGHS_DEVEPTISR[ep], UOTGHS_DEVEPTISR_TXINI) == 0U)
        {
            break;
        }

        uint32_t chunk = length - sent;

        if (ep == EP0)
        {
            const uint32_t available = max_packet - g_send_fifo_ptr[ep];

            if (chunk > available)
            {
                chunk = available;
            }
        }
        else
        {
            g_send_fifo_ptr[ep] = 0U;

            if (chunk > max_packet)
            {
                chunk = max_packet;
            }
        }

        for (uint32_t i = 0U; i < chunk; ++i)
        {
            fifo[g_send_fifo_ptr[ep] + i] = source[sent + i];
        }

        g_send_fifo_ptr[ep] += chunk;
        sent += chunk;

        if ((ep != EP0) || (g_send_fifo_ptr[ep] == max_packet))
        {
            usb_release_tx(ep);
        }
    }

    return sent;
}

static void usb_clear_in(void)
{
    usb_release_tx(EP0);
}

static void usb_clear_out(void)
{
    UOTGHS->UOTGHS_DEVEPTICR[EP0] = UOTGHS_DEVEPTICR_RXOUTIC;
    g_recv_fifo_ptr[EP0] = 0U;
}

static void usb_clear_setup(void)
{
    UOTGHS->UOTGHS_DEVEPTICR[EP0] = UOTGHS_DEVEPTICR_RXSTPIC;
    g_recv_fifo_ptr[EP0] = 0U;
}

static void usb_stall(void)
{
#ifdef UOTGHS_DEVEPTIER_STALLRQS
    UOTGHS->UOTGHS_DEVEPTIER[EP0] = UOTGHS_DEVEPTIER_STALLRQS;
#else
    UOTGHS->UOTGHS_DEVEPT = UOTGHS_DEVEPT_EPEN0;
#endif
}

static uint32_t usb_received_setup(void)
{
    return (UOTGHS->UOTGHS_DEVEPTISR[EP0] & UOTGHS_DEVEPTISR_RXSTPI);
}

static uint32_t usb_recv_control(void* data, uint32_t length)
{
    if (usb_wait_for_flag(&UOTGHS->UOTGHS_DEVEPTISR[EP0], UOTGHS_DEVEPTISR_RXOUTI) == 0U)
    {
        return 0U;
    }

    usb_recv(EP0, static_cast<uint8_t*>(data), length);
    usb_clear_out();

    return length;
}

static uint32_t usb_send_control(const void* data, uint32_t length)
{
    const uint32_t sent = usb_send(EP0, data, length);
    return sent;
}

static uint32_t usb_send_string_descriptor(const char* text, uint16_t max_length)
{
    uint8_t buffer[64];
    uint32_t index = 2U;

    if ((text == 0) || (max_length < 2U))
    {
        return 0U;
    }

    while ((*text != '\0') && ((index + 1U) < sizeof(buffer)) && ((index + 1U) < max_length))
    {
        buffer[index++] = static_cast<uint8_t>(*text++);
        buffer[index++] = 0U;
    }

    buffer[0] = static_cast<uint8_t>(index);
    buffer[1] = USB_STRING_DESCRIPTOR_TYPE;

    return usb_send_control(buffer, index);
}

static uint32_t usb_send_language_descriptor(void)
{
    static const uint8_t language_descriptor[] =
    {
        4U,
        USB_STRING_DESCRIPTOR_TYPE,
        0x09U,
        0x04U
    };

    return usb_send_control(language_descriptor, sizeof(language_descriptor));
}

static uint32_t usb_send_configuration(uint16_t max_length, uint8_t other_speed)
{
    uint8_t buffer[sizeof(ConfigDescriptor) + sizeof(CdcDescriptor)];
    ConfigDescriptor config;

    config.len = 9U;
    config.dtype = other_speed ? USB_OTHER_SPEED_CONFIGURATION : USB_CONFIGURATION_DESCRIPTOR_TYPE;
    config.clen = static_cast<uint16_t>(sizeof(buffer));
    config.numInterfaces = 2U;
    config.config = 1U;
    config.iconfig = 0U;
    config.attributes = 0xC0U;
    config.maxPower = 250U;

    memcpy(buffer, &config, sizeof(config));

    if (other_speed != 0U)
    {
        memcpy(buffer + sizeof(config), &g_cdc_other_speed_descriptor, sizeof(g_cdc_other_speed_descriptor));
    }
    else
    {
        memcpy(buffer + sizeof(config), &g_cdc_descriptor, sizeof(g_cdc_descriptor));
    }

    uint16_t length = static_cast<uint16_t>(sizeof(buffer));

    if (max_length < length)
    {
        length = max_length;
    }

    return usb_send_control(buffer, length);
}

static uint32_t usb_send_descriptor(const UsbSetup& setup)
{
    const uint8_t descriptor_type = setup.wValueH;

    if (descriptor_type == USB_CONFIGURATION_DESCRIPTOR_TYPE)
    {
        return usb_send_configuration(setup.wLength, 0U);
    }

    if (descriptor_type == USB_OTHER_SPEED_CONFIGURATION)
    {
        return usb_send_configuration(setup.wLength, 1U);
    }

    if (descriptor_type == USB_DEVICE_DESCRIPTOR_TYPE)
    {
        const DeviceDescriptor* descriptor = (g_usb_composite != 0U) ? &g_device_descriptor_iad : &g_device_descriptor;

        if (setup.wLength == 8U)
        {
            g_usb_composite = 1U;
        }

        uint16_t length = descriptor->len;

        if (setup.wLength < length)
        {
            length = setup.wLength;
        }

        return usb_send_control(descriptor, length);
    }

    if (descriptor_type == USB_DEVICE_QUALIFIER)
    {
        uint16_t length = g_device_qualifier.len;

        if (setup.wLength < length)
        {
            length = setup.wLength;
        }

        return usb_send_control(&g_device_qualifier, length);
    }

    if (descriptor_type == USB_STRING_DESCRIPTOR_TYPE)
    {
        if (setup.wValueL == 0U)
        {
            return usb_send_language_descriptor();
        }

        if (setup.wValueL == IMANUFACTURER)
        {
            return usb_send_string_descriptor(HAL_USB_CDC_MANUFACTURER_NAME, setup.wLength);
        }

        if (setup.wValueL == IPRODUCT)
        {
            return usb_send_string_descriptor(HAL_USB_CDC_PRODUCT_NAME, setup.wLength);
        }

        if (setup.wValueL == ISERIAL)
        {
            return usb_send_string_descriptor("LCP", setup.wLength);
        }
    }

    return 0U;
}

static void usb_ring_accept(void)
{
    while (usb_endpoint_fifo_byte_count(CDC_RX) != 0U)
    {
        const uint32_t next = (g_rx_head + 1U) % CDC_RX_BUFFER_SIZE;

        if (next == g_rx_tail)
        {
            break;
        }

        g_rx_buffer[g_rx_head] = usb_recv8(CDC_RX);
        g_rx_head = next;
    }

    if (usb_endpoint_fifo_byte_count(CDC_RX) == 0U)
    {
        usb_release_rx(CDC_RX);
    }
}

static uint8_t cdc_setup(const UsbSetup& setup)
{
    if (setup.bmRequestType == REQUEST_DEVICETOHOST_CLASS_INTERFACE)
    {
        if (setup.bRequest == CDC_GET_LINE_CODING)
        {
            LineInfo line_info;
            line_info.dwDTERate = g_line_info.dwDTERate;
            line_info.bCharFormat = g_line_info.bCharFormat;
            line_info.bParityType = g_line_info.bParityType;
            line_info.bDataBits = g_line_info.bDataBits;
            line_info.lineState = g_line_info.lineState;

            return (usb_send_control(&line_info, 7U) == 7U) ? 1U : 0U;
        }
    }

    if (setup.bmRequestType == REQUEST_HOSTTODEVICE_CLASS_INTERFACE)
    {
        if (setup.bRequest == CDC_SET_LINE_CODING)
        {
            LineInfo line_info;

            if (usb_recv_control(&line_info, 7U) != 7U)
            {
                return 0U;
            }

            g_line_info.dwDTERate = line_info.dwDTERate;
            g_line_info.bCharFormat = line_info.bCharFormat;
            g_line_info.bParityType = line_info.bParityType;
            g_line_info.bDataBits = line_info.bDataBits;

            return 1U;
        }

        if (setup.bRequest == CDC_SET_CONTROL_LINE_STATE)
        {
            g_line_info.lineState = setup.wValueL;

            if (g_line_info.dwDTERate == 1200U)
            {
                if ((g_line_info.lineState & CDC_LINESTATE_DTR) == 0U)
                {
                    sam3x_bootloader_request(USB_BOOTLOADER_DELAY_MS);
                }
                else
                {
                    sam3x_bootloader_cancel();
                }
            }

            return 1U;
        }

        if (setup.bRequest == CDC_SEND_BREAK)
        {
            g_break_value = static_cast<int32_t>((static_cast<uint16_t>(setup.wValueH) << 8) | setup.wValueL);
            return 1U;
        }
    }

    return 0U;
}

static void usb_configure_endpoint(uint32_t ep, uint32_t config)
{
    UOTGHS->UOTGHS_DEVEPTCFG[ep] = config;
    UOTGHS->UOTGHS_DEVEPT |= (1UL << ep);

#ifdef UOTGHS_DEVEPTCFG_ALLOC
    UOTGHS->UOTGHS_DEVEPTCFG[ep] |= UOTGHS_DEVEPTCFG_ALLOC;
#endif

#ifdef UOTGHS_DEVEPTISR_CFGOK
    if ((UOTGHS->UOTGHS_DEVEPTISR[ep] & UOTGHS_DEVEPTISR_CFGOK) == 0U)
    {
        g_usb_error = 1U;
    }
#endif
}

static uint32_t usb_endpoint_config_control(void)
{
    return UOTGHS_DEVEPTCFG_EPSIZE_64_BYTE
         | UOTGHS_DEVEPTCFG_EPTYPE_CTRL
         | UOTGHS_DEVEPTCFG_EPBK_1_BANK
         | UOTGHS_DEVEPTCFG_ALLOC;
}

static uint32_t usb_endpoint_config_interrupt_in(void)
{
    return UOTGHS_DEVEPTCFG_EPSIZE_64_BYTE
         | UOTGHS_DEVEPTCFG_EPDIR_IN
         | UOTGHS_DEVEPTCFG_EPTYPE_INTRPT
         | UOTGHS_DEVEPTCFG_EPBK_1_BANK
         | UOTGHS_DEVEPTCFG_ALLOC;
}

static uint32_t usb_endpoint_config_bulk_out(uint32_t size_512)
{
    return (size_512 ? UOTGHS_DEVEPTCFG_EPSIZE_512_BYTE : UOTGHS_DEVEPTCFG_EPSIZE_64_BYTE)
         | UOTGHS_DEVEPTCFG_EPTYPE_BLK
         | UOTGHS_DEVEPTCFG_EPBK_2_BANK
         | UOTGHS_DEVEPTCFG_ALLOC;
}

static uint32_t usb_endpoint_config_bulk_in(uint32_t size_512)
{
    return (size_512 ? UOTGHS_DEVEPTCFG_EPSIZE_512_BYTE : UOTGHS_DEVEPTCFG_EPSIZE_64_BYTE)
         | UOTGHS_DEVEPTCFG_EPDIR_IN
         | UOTGHS_DEVEPTCFG_EPTYPE_BLK
         | UOTGHS_DEVEPTCFG_EPBK_2_BANK
         | UOTGHS_DEVEPTCFG_ALLOC;
}

static void usb_init_endpoints(void)
{
    usb_configure_endpoint(EP0, usb_endpoint_config_control());
    UOTGHS->UOTGHS_DEVEPTIER[EP0] = UOTGHS_DEVEPTIER_RXSTPES;

    usb_configure_endpoint(CDC_ENDPOINT_ACM, usb_endpoint_config_interrupt_in());
    usb_configure_endpoint(CDC_ENDPOINT_OUT, usb_endpoint_config_bulk_out(1U));
    usb_configure_endpoint(CDC_ENDPOINT_IN, usb_endpoint_config_bulk_in(1U));

    UOTGHS->UOTGHS_DEVEPTIER[CDC_RX] = UOTGHS_DEVEPTIER_RXOUTES;
}

static void usb_reset_handler(void)
{
    UOTGHS->UOTGHS_DEVCTRL &= ~UOTGHS_DEVCTRL_UADD_Msk;
    UOTGHS->UOTGHS_DEVCTRL |= UOTGHS_DEVCTRL_ADDEN;

    usb_init_endpoints();

    g_usb_configuration = 0U;
    UOTGHS->UOTGHS_DEVICR = UOTGHS_DEVICR_EORSTC;
}

static void usb_setup_handler(void)
{
    UsbSetup setup;

    if (usb_received_setup() == 0U)
    {
        return;
    }

    usb_recv(EP0, reinterpret_cast<uint8_t*>(&setup), sizeof(setup));
    usb_clear_setup();

    if ((setup.bmRequestType & REQUEST_DEVICETOHOST) != 0U)
    {
        (void)usb_wait_for_flag(&UOTGHS->UOTGHS_DEVEPTISR[EP0], UOTGHS_DEVEPTISR_TXINI);
    }
    else
    {
        usb_clear_in();
    }

    uint8_t ok = 1U;

    if ((setup.bmRequestType & REQUEST_TYPE) == REQUEST_STANDARD)
    {
        const uint8_t request = setup.bRequest;

        if (request == GET_STATUS)
        {
            usb_send8(EP0, 0U);
            usb_send8(EP0, 0U);
        }
        else if ((request == CLEAR_FEATURE) || (request == SET_FEATURE))
        {
            usb_send8(EP0, 0U);
        }
        else if (request == SET_ADDRESS)
        {
            (void)usb_wait_for_flag(&UOTGHS->UOTGHS_DEVEPTISR[EP0], UOTGHS_DEVEPTISR_TXINI);
            UOTGHS->UOTGHS_DEVCTRL = (UOTGHS->UOTGHS_DEVCTRL & ~UOTGHS_DEVCTRL_UADD_Msk)
                                   | UOTGHS_DEVCTRL_UADD(setup.wValueL)
                                   | UOTGHS_DEVCTRL_ADDEN;
        }
        else if (request == GET_DESCRIPTOR)
        {
            ok = (usb_send_descriptor(setup) > 0U) ? 1U : 0U;
        }
        else if (request == SET_DESCRIPTOR)
        {
            ok = 0U;
        }
        else if (request == GET_CONFIGURATION)
        {
            usb_send8(EP0, static_cast<uint8_t>(g_usb_configuration));
        }
        else if (request == SET_CONFIGURATION)
        {
            if (REQUEST_DEVICE == (setup.bmRequestType & REQUEST_RECIPIENT))
            {
                usb_init_endpoints();
                g_usb_configuration = setup.wValueL;
            }
            else
            {
                ok = 0U;
            }
        }
        else if (request == GET_INTERFACE)
        {
            usb_send8(EP0, static_cast<uint8_t>(g_usb_set_interface));
        }
        else if (request == SET_INTERFACE)
        {
            g_usb_set_interface = setup.wValueL;
        }
        else
        {
            ok = 0U;
        }
    }
    else
    {
        ok = cdc_setup(setup);
    }

    if (ok != 0U)
    {
        usb_clear_in();
    }
    else
    {
        usb_stall();
    }
}

static void usb_interrupt_handler(void)
{
    const uint32_t status = UOTGHS->UOTGHS_DEVISR;

#ifdef UOTGHS_DEVISR_EORST
    if ((status & UOTGHS_DEVISR_EORST) != 0U)
    {
        usb_reset_handler();
    }
#endif

#ifdef UOTGHS_DEVISR_PEP_2
    if ((status & UOTGHS_DEVISR_PEP_2) != 0U)
    {
        if ((UOTGHS->UOTGHS_DEVEPTISR[CDC_RX] & UOTGHS_DEVEPTISR_RXOUTI) != 0U)
        {
            usb_ring_accept();
        }
    }
#endif

#ifdef UOTGHS_DEVISR_PEP_0
    if ((status & UOTGHS_DEVISR_PEP_0) != 0U)
    {
        usb_setup_handler();
    }
#endif
}

extern "C" void UOTGHS_Handler(void)
{
    usb_interrupt_handler();
}

void hal_usb_cdc_begin(void)
{
    if (g_usb_initialized == 0U)
    {
        for (uint32_t i = 0U; i < USB_ENDPOINT_COUNT; ++i)
        {
            g_send_fifo_ptr[i] = 0U;
            g_recv_fifo_ptr[i] = 0U;
        }

        g_rx_head = 0U;
        g_rx_tail = 0U;
        g_usb_error = 0U;
        g_usb_configuration = 0U;

        usb_enable_clocks();
        usb_force_device_mode();
        usb_enable_pad_and_clock();

        NVIC_SetPriority(UOTGHS_IRQn, 0U);
        NVIC_EnableIRQ(UOTGHS_IRQn);

#ifdef UOTGHS_DEVIER_EORSTES
        UOTGHS->UOTGHS_DEVIER = UOTGHS_DEVIER_EORSTES;
#endif
#ifdef UOTGHS_DEVIER_PEP_0
        UOTGHS->UOTGHS_DEVIER = UOTGHS_DEVIER_PEP_0;
#endif
#ifdef UOTGHS_DEVIER_PEP_2
        UOTGHS->UOTGHS_DEVIER = UOTGHS_DEVIER_PEP_2;
#endif

        g_usb_initialized = 1U;
    }

    UOTGHS->UOTGHS_DEVCTRL &= ~UOTGHS_DEVCTRL_DETACH;
}

void hal_usb_cdc_end(void)
{
    if (g_usb_initialized != 0U)
    {
        UOTGHS->UOTGHS_DEVCTRL |= UOTGHS_DEVCTRL_DETACH;
        usb_freeze_clock();
        g_usb_configuration = 0U;
    }
}

uint8_t hal_usb_cdc_configured(void)
{
    return (g_usb_configuration != 0U) ? 1U : 0U;
}

uint8_t hal_usb_cdc_opened(void)
{
    return ((g_line_info.lineState & (CDC_LINESTATE_DTR | CDC_LINESTATE_RTS)) != 0U) ? 1U : 0U;
}

int hal_usb_cdc_available(void)
{
    return static_cast<int>((CDC_RX_BUFFER_SIZE + g_rx_head - g_rx_tail) % CDC_RX_BUFFER_SIZE);
}

int hal_usb_cdc_available_for_write(void)
{
    return static_cast<int>(EPX_SIZE - 1U);
}

int hal_usb_cdc_read(void)
{
    if (g_rx_head == g_rx_tail)
    {
        return -1;
    }

    const uint8_t value = g_rx_buffer[g_rx_tail];
    g_rx_tail = (g_rx_tail + 1U) % CDC_RX_BUFFER_SIZE;

    if (usb_endpoint_fifo_byte_count(CDC_RX) != 0U)
    {
        usb_ring_accept();
    }

    return value;
}

int hal_usb_cdc_peek(void)
{
    if (g_rx_head == g_rx_tail)
    {
        return -1;
    }

    return g_rx_buffer[g_rx_tail];
}

size_t hal_usb_cdc_write(const uint8_t* buffer, size_t size)
{
    if ((buffer == 0) || (size == 0U))
    {
        return 0U;
    }

    if ((g_usb_configuration == 0U) || (hal_usb_cdc_opened() == 0U))
    {
        return 0U;
    }

    const uint32_t written = usb_send(CDC_TX, buffer, static_cast<uint32_t>(size));

    return static_cast<size_t>(written);
}

void hal_usb_cdc_flush(void)
{
    if (g_usb_configuration != 0U)
    {
        usb_release_tx(CDC_TX);
    }
}

uint32_t hal_usb_cdc_baud(void)
{
    return g_line_info.dwDTERate;
}

uint8_t hal_usb_cdc_stopbits(void)
{
    return g_line_info.bCharFormat;
}

uint8_t hal_usb_cdc_paritytype(void)
{
    return g_line_info.bParityType;
}

uint8_t hal_usb_cdc_numbits(void)
{
    return g_line_info.bDataBits;
}

uint8_t hal_usb_cdc_dtr(void)
{
    return ((g_line_info.lineState & CDC_LINESTATE_DTR) != 0U) ? 1U : 0U;
}

uint8_t hal_usb_cdc_rts(void)
{
    return ((g_line_info.lineState & CDC_LINESTATE_RTS) != 0U) ? 1U : 0U;
}

uint8_t hal_usb_cdc_error(void)
{
    return (g_usb_error != 0U) ? 1U : 0U;
}

void hal_usb_cdc_clear_error(void)
{
    g_usb_error = 0U;
}
