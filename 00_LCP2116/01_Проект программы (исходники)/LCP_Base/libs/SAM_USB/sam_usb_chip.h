
/**
 * @file sam_usb_chip.h
 * @brief Минимальная аппаратная обвязка UOTGHS для ATSAM3X8E.
 */

#ifndef SAM_USB_CHIP_H
#define SAM_USB_CHIP_H

#include <stdint.h>

#include "sam.h"

#ifndef SAM3XA_SERIES
#define SAM3XA_SERIES 1

static inline void udd_ack_fifocon(uint32_t endpoint)
{
#if defined(UOTGHS_DEVEPTIDR_FIFOCONC)
    UOTGHS->UOTGHS_DEVEPTIDR[endpoint] = UOTGHS_DEVEPTIDR_FIFOCONC;
#endif
}

#ifdef __cplusplus
extern "C" {
#endif

void UDD_SetStack(void (*pf_isr)(void));
uint32_t UDD_Init(void);
void UDD_Attach(void);
void UDD_Detach(void);
void UDD_InitEP(uint32_t ul_ep_nb, uint32_t ul_ep_cfg);
void UDD_InitEndpoints(const uint32_t* eps_table, const uint32_t ul_eps_table_size);
void UDD_WaitIN(void);
void UDD_WaitOUT(void);
uint32_t UDD_WaitForINOrOUT(void);
void UDD_ClearIN(void);
void UDD_ClearOUT(void);
uint32_t UDD_ReceivedSetupInt(void);
void UDD_ClearSetupInt(void);
uint32_t UDD_Send(uint32_t ep, const void* data, uint32_t len);
void UDD_Send8(uint32_t ep, uint8_t data);
void UDD_Recv(uint32_t ep, uint8_t* data, uint32_t len);
uint8_t UDD_Recv8(uint32_t ep);
void UDD_ReleaseRX(uint32_t ep);
void UDD_ReleaseTX(uint32_t ep);
uint32_t UDD_FifoByteCount(uint32_t ep);
uint32_t UDD_ReadWriteAllowed(uint32_t ep);
void UDD_Stall(void);
void UDD_SetAddress(uint32_t addr);
uint32_t UDD_GetFrameNumber(void);

#ifdef __cplusplus
}
#endif

#endif

#ifndef MAX_ENDPOINTS
#define MAX_ENDPOINTS 10U
#endif

#ifndef EP0
#define EP0 0U
#endif

#ifndef EP0_SIZE
#define EP0_SIZE 64U
#endif

#ifndef EPX_SIZE
#define EPX_SIZE 512U
#endif

#ifndef UOTGHS_RAM_ADDR
#define UOTGHS_RAM_ADDR 0x20100000U
#endif

#ifndef UOTGHS_ENDPOINT_FIFO_STRIDE
#define UOTGHS_ENDPOINT_FIFO_STRIDE 0x8000U
#endif

#ifndef EP_TYPE_CONTROL
#define EP_TYPE_CONTROL (UOTGHS_DEVEPTCFG_EPSIZE_64_BYTE | UOTGHS_DEVEPTCFG_EPTYPE_CTRL | UOTGHS_DEVEPTCFG_EPBK_1_BANK | UOTGHS_DEVEPTCFG_ALLOC)
#endif

#ifndef EP_TYPE_INTERRUPT_IN
#define EP_TYPE_INTERRUPT_IN (UOTGHS_DEVEPTCFG_EPSIZE_64_BYTE | UOTGHS_DEVEPTCFG_EPDIR_IN | UOTGHS_DEVEPTCFG_EPTYPE_INTRPT | UOTGHS_DEVEPTCFG_EPBK_1_BANK | UOTGHS_DEVEPTCFG_ALLOC)
#endif

#ifndef EP_TYPE_BULK_OUT
#define EP_TYPE_BULK_OUT (UOTGHS_DEVEPTCFG_EPSIZE_512_BYTE | UOTGHS_DEVEPTCFG_EPTYPE_BLK | UOTGHS_DEVEPTCFG_EPBK_2_BANK | UOTGHS_DEVEPTCFG_ALLOC)
#endif

#ifndef EP_TYPE_BULK_IN
#define EP_TYPE_BULK_IN (UOTGHS_DEVEPTCFG_EPSIZE_512_BYTE | UOTGHS_DEVEPTCFG_EPDIR_IN | UOTGHS_DEVEPTCFG_EPTYPE_BLK | UOTGHS_DEVEPTCFG_EPBK_2_BANK | UOTGHS_DEVEPTCFG_ALLOC)
#endif

typedef uint32_t irqflags_t;

static inline irqflags_t cpu_irq_save(void)
{
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static inline void cpu_irq_restore(irqflags_t flags)
{
    if ((flags & 1U) == 0U)
    {
        __enable_irq();
    }
}

static inline void pmc_enable_periph_clk(uint32_t peripheral_id)
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

static inline void pmc_enable_upll_clock(void)
{
#if defined(CKGR_UCKR_UPLLEN) && defined(PMC_SR_LOCKU)
    PMC->CKGR_UCKR = CKGR_UCKR_UPLLCOUNT(3U) | CKGR_UCKR_UPLLEN;

    while ((PMC->PMC_SR & PMC_SR_LOCKU) == 0U)
    {
    }
#endif
}

static inline void pmc_switch_udpck_to_upllck(uint32_t divider)
{
    (void)divider;

#if defined(PMC_USB_USBS) && defined(PMC_USB_USBDIV)
    PMC->PMC_USB = PMC_USB_USBS | PMC_USB_USBDIV(0U);
#endif
}

static inline void pmc_enable_udpck(void)
{
#if defined(PMC_SCER_UDP)
    PMC->PMC_SCER = PMC_SCER_UDP;
#endif
}

static inline void otg_disable_id_pin(void)
{
#if defined(UOTGHS_CTRL_UIDE)
    UOTGHS->UOTGHS_CTRL &= ~UOTGHS_CTRL_UIDE;
#endif
}

static inline void otg_force_device_mode(void)
{
#if defined(UOTGHS_CTRL_UIMOD)
    UOTGHS->UOTGHS_CTRL |= UOTGHS_CTRL_UIMOD;
#endif
}

static inline void otg_disable_pad(void)
{
#if defined(UOTGHS_CTRL_OTGPADE)
    UOTGHS->UOTGHS_CTRL &= ~UOTGHS_CTRL_OTGPADE;
#endif
}

static inline void otg_enable_pad(void)
{
#if defined(UOTGHS_CTRL_OTGPADE)
    UOTGHS->UOTGHS_CTRL |= UOTGHS_CTRL_OTGPADE;
#endif
}

static inline void otg_enable(void)
{
#if defined(UOTGHS_CTRL_USBE)
    UOTGHS->UOTGHS_CTRL |= UOTGHS_CTRL_USBE;
#endif
}

static inline void otg_unfreeze_clock(void)
{
#if defined(UOTGHS_CTRL_FRZCLK)
    UOTGHS->UOTGHS_CTRL &= ~UOTGHS_CTRL_FRZCLK;
#endif
}

static inline void otg_freeze_clock(void)
{
#if defined(UOTGHS_CTRL_FRZCLK)
    UOTGHS->UOTGHS_CTRL |= UOTGHS_CTRL_FRZCLK;
#endif
}

static inline uint32_t Is_otg_clock_usable(void)
{
#if defined(UOTGHS_SR_CLKUSABLE)
    return (UOTGHS->UOTGHS_SR & UOTGHS_SR_CLKUSABLE) != 0U;
#else
    return 1U;
#endif
}

static inline uint32_t Is_otg_vbus_high(void)
{
#if defined(UOTGHS_SR_VBUS)
    return (UOTGHS->UOTGHS_SR & UOTGHS_SR_VBUS) != 0U;
#else
    return 1U;
#endif
}

static inline void otg_ack_vbus_transition(void)
{
#if defined(UOTGHS_SCR_VBUSTIC)
    UOTGHS->UOTGHS_SCR = UOTGHS_SCR_VBUSTIC;
#endif
}

static inline void otg_raise_vbus_transition(void)
{
#if defined(UOTGHS_SFR_VBUSTI)
    UOTGHS->UOTGHS_SFR = UOTGHS_SFR_VBUSTI;
#endif
}

static inline void otg_enable_vbus_interrupt(void)
{
#if defined(UOTGHS_CTRL_VBUSTE)
    UOTGHS->UOTGHS_CTRL |= UOTGHS_CTRL_VBUSTE;
#endif
}

static inline void udd_low_speed_disable(void)
{
#if defined(UOTGHS_DEVCTRL_LS)
    UOTGHS->UOTGHS_DEVCTRL &= ~UOTGHS_DEVCTRL_LS;
#endif
}

static inline void udd_high_speed_enable(void)
{
#if defined(UOTGHS_DEVCTRL_SPDCONF_0)
    UOTGHS->UOTGHS_DEVCTRL |= UOTGHS_DEVCTRL_SPDCONF_0;
#endif
}

static inline void udd_attach_device(void)
{
#if defined(UOTGHS_DEVCTRL_DETACH)
    UOTGHS->UOTGHS_DEVCTRL &= ~UOTGHS_DEVCTRL_DETACH;
#endif
}

static inline uint32_t Is_udd_reset(void)
{
#if defined(UOTGHS_DEVISR_EORST)
    return (UOTGHS->UOTGHS_DEVISR & UOTGHS_DEVISR_EORST) != 0U;
#else
    return 0U;
#endif
}

static inline void udd_ack_reset(void)
{
#if defined(UOTGHS_DEVICR_EORSTC)
    UOTGHS->UOTGHS_DEVICR = UOTGHS_DEVICR_EORSTC;
#endif
}

static inline void udd_enable_reset_interrupt(void)
{
#if defined(UOTGHS_DEVIER_EORSTES)
    UOTGHS->UOTGHS_DEVIER = UOTGHS_DEVIER_EORSTES;
#endif
}

static inline uint32_t Is_udd_sof(void)
{
#if defined(UOTGHS_DEVISR_SOF)
    return (UOTGHS->UOTGHS_DEVISR & UOTGHS_DEVISR_SOF) != 0U;
#else
    return 0U;
#endif
}

static inline void udd_ack_sof(void)
{
#if defined(UOTGHS_DEVICR_SOFC)
    UOTGHS->UOTGHS_DEVICR = UOTGHS_DEVICR_SOFC;
#endif
}

static inline void udd_enable_sof_interrupt(void)
{
#if defined(UOTGHS_DEVIER_SOFES)
    UOTGHS->UOTGHS_DEVIER = UOTGHS_DEVIER_SOFES;
#endif
}

static inline void udd_configure_address(uint32_t address)
{
#if defined(UOTGHS_DEVCTRL_UADD_Msk) && defined(UOTGHS_DEVCTRL_UADD)
    UOTGHS->UOTGHS_DEVCTRL = (UOTGHS->UOTGHS_DEVCTRL & ~UOTGHS_DEVCTRL_UADD_Msk) | UOTGHS_DEVCTRL_UADD(address);
#endif
}

static inline void udd_enable_address(void)
{
#if defined(UOTGHS_DEVCTRL_ADDEN)
    UOTGHS->UOTGHS_DEVCTRL |= UOTGHS_DEVCTRL_ADDEN;
#endif
}

static inline void udd_enable_endpoint(uint32_t endpoint)
{
    UOTGHS->UOTGHS_DEVEPT |= (1UL << endpoint);
}

static inline uint32_t Is_udd_endpoint_configured(uint32_t endpoint)
{
#if defined(UOTGHS_DEVEPTISR_CFGOK)
    return (UOTGHS->UOTGHS_DEVEPTISR[endpoint] & UOTGHS_DEVEPTISR_CFGOK) != 0U;
#else
    return 1U;
#endif
}

static inline void udd_enable_endpoint_interrupt(uint32_t endpoint)
{
#if defined(UOTGHS_DEVIER_PEP_0)
    UOTGHS->UOTGHS_DEVIER = (UOTGHS_DEVIER_PEP_0 << endpoint);
#endif
}

static inline uint32_t Is_udd_endpoint_interrupt(uint32_t endpoint)
{
#if defined(UOTGHS_DEVISR_PEP_0)
    return (UOTGHS->UOTGHS_DEVISR & (UOTGHS_DEVISR_PEP_0 << endpoint)) != 0U;
#else
    return 0U;
#endif
}

static inline void udd_enable_setup_received_interrupt(uint32_t endpoint)
{
#if defined(UOTGHS_DEVEPTIER_RXSTPES)
    UOTGHS->UOTGHS_DEVEPTIER[endpoint] = UOTGHS_DEVEPTIER_RXSTPES;
#endif
}

static inline void udd_enable_out_received_interrupt(uint32_t endpoint)
{
#if defined(UOTGHS_DEVEPTIER_RXOUTES)
    UOTGHS->UOTGHS_DEVEPTIER[endpoint] = UOTGHS_DEVEPTIER_RXOUTES;
#endif
}

static inline void udd_ack_out_received(uint32_t endpoint)
{
#if defined(UOTGHS_DEVEPTICR_RXOUTIC)
    UOTGHS->UOTGHS_DEVEPTICR[endpoint] = UOTGHS_DEVEPTICR_RXOUTIC;
#endif
}

static inline uint32_t udd_frame_number(void)
{
#if defined(UOTGHS_DEVFNUM_FNUM_Msk)
    return UOTGHS->UOTGHS_DEVFNUM & UOTGHS_DEVFNUM_FNUM_Msk;
#else
    return UOTGHS->UOTGHS_DEVFNUM;
#endif
}

static inline volatile uint8_t* udd_get_endpoint_fifo_access8(uint32_t endpoint)
{
    return (volatile uint8_t*)(UOTGHS_RAM_ADDR + (endpoint * UOTGHS_ENDPOINT_FIFO_STRIDE));
}

#endif
