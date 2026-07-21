/**
 * @file x2x_module_drivers.hpp
 * @brief Публичные entry points штатных драйверов модулей X2X.
 *
 * Каждая функция соответствует сигнатуре X2XModulePollFunction и регистрируется
 * ровно один раз в x2x_catalog.cpp. Драйверы остаются разделёнными по типам,
 * чтобы аппаратные карты регистров и state machine были видны явно.
 */

#pragma once

#include "../x2x_module.hpp"

/** @copydoc X2XModulePollFunction */
X2XModulePollResult x2x_module_poll_ldo1118(X2XDeviceHeader* device,
                                             X2XModuleContext& context);

/** @copydoc X2XModulePollFunction */
X2XModulePollResult x2x_module_poll_lai1118(X2XDeviceHeader* device,
                                             X2XModuleContext& context);

/** @copydoc X2XModulePollFunction */
X2XModulePollResult x2x_module_poll_ldi1118(X2XDeviceHeader* device,
                                             X2XModuleContext& context);

/** @copydoc X2XModulePollFunction */
X2XModulePollResult x2x_module_poll_ldi1116(X2XDeviceHeader* device,
                                             X2XModuleContext& context);

/** @copydoc X2XModulePollFunction */
X2XModulePollResult x2x_module_poll_lct1114(X2XDeviceHeader* device,
                                             X2XModuleContext& context);

/** @copydoc X2XModulePollFunction */
X2XModulePollResult x2x_module_poll_lct1114_2(X2XDeviceHeader* device,
                                               X2XModuleContext& context);
