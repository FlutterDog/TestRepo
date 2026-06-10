
/**
 * @file sam_usb_platform.hpp
 * @brief Платформенная обвязка для USB CDC стека ATSAM3X8E.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sam_usb_chip.h"
#include "../../board/lcp_usb_identity.hpp"
#include "../../platform/print.hpp"

#ifndef USBCON
#define USBCON 1
#endif

#ifndef WEAK
#define WEAK __attribute__ ((weak))
#endif

#ifndef USB_VID
#define USB_VID LCP_USB_SERVICE_VID
#endif

#ifndef USB_PID
#define USB_PID LCP_USB_SERVICE_PID
#endif

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

uint32_t millis(void);
void delay(uint32_t ms);
