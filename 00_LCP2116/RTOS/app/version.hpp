/**
 * @file version.hpp
 * @brief Версия базовой прошивки LCP.
 *
 * Номер изменяется для каждого аппаратного verification candidate. Release tag
 * и merge выполняются отдельно только после подтверждения Debug/Release build
 * и финального прогона на LCP2116.
 */

#pragma once

#define LCP_DIAGNOSTIC_SOFTWARE_NAME "LCP Basic Diagnostic Firmware"
#define LCP_DIAGNOSTIC_SOFTWARE_VERSION "0.18.3"
#define LCP_DIAGNOSTIC_SOFTWARE_STAGE "Stage 18D: Final code and console review"
#define LCP_DIAGNOSTIC_SOFTWARE_TARGET "ATSAM3X8E"
