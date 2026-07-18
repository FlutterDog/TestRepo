/**
 * @file x2x_module_drivers.hpp
 * @brief Объявления драйверов штатных модулей X2X.
 */

#pragma once

#include "../x2x_module.hpp"

/** @brief Выполняет один неблокирующий шаг опроса LDO1118. */
X2XModulePollResult x2x_module_poll_ldo1118(X2XDeviceHeader* device,
                                            X2XModuleContext& context);

/** @brief Выполняет один неблокирующий шаг опроса LAI1118. */
X2XModulePollResult x2x_module_poll_lai1118(X2XDeviceHeader* device,
                                            X2XModuleContext& context);

/** @brief Выполняет один неблокирующий шаг опроса LDI1118. */
X2XModulePollResult x2x_module_poll_ldi1118(X2XDeviceHeader* device,
                                            X2XModuleContext& context);

/** @brief Выполняет один неблокирующий шаг опроса LDI1116. */
X2XModulePollResult x2x_module_poll_ldi1116(X2XDeviceHeader* device,
                                            X2XModuleContext& context);

/** @brief Выполняет один неблокирующий шаг опроса LCT1114. */
X2XModulePollResult x2x_module_poll_lct1114(X2XDeviceHeader* device,
                                            X2XModuleContext& context);

/** @brief Выполняет один неблокирующий шаг опроса LCT1114 второй версии. */
X2XModulePollResult x2x_module_poll_lct1114_2(X2XDeviceHeader* device,
                                              X2XModuleContext& context);
