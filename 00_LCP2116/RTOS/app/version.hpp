/**
 * @file version.hpp
 * @brief Версия базовой прошивки LCP.
 *
 * Номер изменяется для каждого аппаратного verification candidate. Release tag
 * и merge выполняются отдельно только после подтверждения сборки, размеров
 * Flash/RAM, аппаратного прогона и актуальности пользовательской документации.
 */

#pragma once

#define LCP_DIAGNOSTIC_SOFTWARE_NAME "LCP Basic Diagnostic Firmware"
#define LCP_DIAGNOSTIC_SOFTWARE_VERSION "1.02.0"
#define LCP_DIAGNOSTIC_SOFTWARE_STAGE "Stage 20A: USB configuration transport"
#define LCP_DIAGNOSTIC_SOFTWARE_TARGET "ATSAM3X8E"
