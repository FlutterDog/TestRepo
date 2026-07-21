/**
 * @file x2x_types.hpp
 * @brief Типы данных модулей внутренней шины X2X контроллера LCP.
 *
 * Числовые идентификаторы устройств являются частью формата X2X.TXT.
 * После выпуска прошивки назначенное значение ID нельзя переиспользовать
 * или изменять, иначе существующие конфигурационные файлы потеряют смысл.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/** Максимальное количество внешних модулей на шине X2X. */
static const uint8_t X2X_MAX_MODULES = 6U;

/** Имя конфигурационного файла X2X в корневом каталоге microSD. */
static const char X2X_CONFIG_FILE_NAME[] = "X2X.TXT";

/** ASDU по умолчанию для будущего слоя IEC 60870-5-104. */
static const uint16_t X2X_DEFAULT_ASDU = 1U;

/** Количество последовательных ошибок до фиксации потери связи. */
static const uint8_t X2X_CONNECTION_LOSS_THRESHOLD = 5U;

/** Максимальное количество отсчётов осциллограммы на одну фазу LCT. */
static const uint16_t X2X_MAX_WAVEFORM_SAMPLES = 400U;

/**
 * @brief Стабильные идентификаторы типов устройств X2X.
 *
 * Значения 0..6 сохранены совместимыми со старой Arduino-прошивкой.
 */
enum X2XDeviceType : uint16_t
{
    X2X_DEVICE_LCP = 0U,
    X2X_DEVICE_LDO1118 = 1U,
    X2X_DEVICE_LAI1118 = 2U,
    X2X_DEVICE_LDI1118 = 3U,
    X2X_DEVICE_LCT1114 = 4U,
    X2X_DEVICE_LDI1116 = 5U,
    X2X_DEVICE_LCT1114_2 = 6U
};

/** @brief Последняя зарегистрированная ошибка обмена с модулем. */
enum X2XCommunicationError : uint8_t
{
    X2X_COMMUNICATION_OK = 0U,
    X2X_COMMUNICATION_TIMEOUT = 1U,
    X2X_COMMUNICATION_CRC_ERROR = 2U,
    X2X_COMMUNICATION_PROTOCOL_ERROR = 3U,
    X2X_COMMUNICATION_EXCEPTION = 4U,
    X2X_COMMUNICATION_TRANSPORT_ERROR = 5U
};

/**
 * @brief Общий заголовок любого экземпляра устройства X2X.
 *
 * Заголовок содержит только runtime-состояние экземпляра. Он не является
 * сетевым пакетом и намеренно не упакован через pragma pack.
 */
struct X2XDeviceHeader
{
    X2XDeviceType type;
    uint16_t asdu;
    uint8_t slave_address;
    uint8_t connection_lost;
    uint8_t device_fault;
    uint8_t consecutive_failures;
    X2XCommunicationError last_communication_error;
    uint8_t last_exception_code;
    uint32_t last_update_ms;
    uint32_t successful_poll_count;
    uint32_t failed_poll_count;
};

/** @brief Runtime-данные центрального модуля LCP. */
struct X2XLcp2116 : X2XDeviceHeader
{
    static const uint8_t SP_COUNT = 0U;
    static const uint8_t ME_COUNT = 0U;
    static const uint8_t TF_COUNT = 0U;

    uint32_t digital_inputs;
};

/** @brief Runtime-данные релейного модуля LDO1118. */
struct X2XLdo1118 : X2XDeviceHeader
{
    static const uint8_t SP_COUNT = 1U;
    static const uint8_t ME_COUNT = 1U;
    static const uint8_t TF_COUNT = 0U;

    uint32_t digital_inputs;

    union
    {
        int16_t output_value;
        int16_t me_values[ME_COUNT];
    };
};

/** @brief Runtime-данные модуля дискретных входов LDI1118, 8 каналов. */
struct X2XLdi1118 : X2XDeviceHeader
{
    static const uint8_t SP_COUNT = 9U;
    static const uint8_t ME_COUNT = 0U;
    static const uint8_t TF_COUNT = 0U;

    uint32_t digital_inputs;
};

/** @brief Runtime-данные модуля дискретных входов LDI1116, 16 каналов. */
struct X2XLdi1116 : X2XDeviceHeader
{
    static const uint8_t SP_COUNT = 17U;
    static const uint8_t ME_COUNT = 0U;
    static const uint8_t TF_COUNT = 0U;

    uint32_t digital_inputs;
};

/** @brief Runtime-данные модуля аналоговых входов LAI1118. */
struct X2XLai1118 : X2XDeviceHeader
{
    static const uint8_t SP_COUNT = 9U;
    static const uint8_t ME_COUNT = 0U;
    static const uint8_t TF_COUNT = 8U;

    uint32_t digital_inputs;

    union
    {
        struct
        {
            float analog_input_01;
            float analog_input_02;
            float analog_input_03;
            float analog_input_04;
            float analog_input_05;
            float analog_input_06;
            float analog_input_07;
            float analog_input_08;
        };
        float tf_values[TF_COUNT];
    };
};

/** @brief Runtime-данные первой версии модуля коммутационного ресурса LCT1114. */
struct X2XLct1114 : X2XDeviceHeader
{
    static const uint8_t SP_COUNT = 20U;
    static const uint8_t ME_COUNT = 0U;
    static const uint8_t TF_COUNT = 42U;

    uint32_t digital_inputs;

    union
    {
        struct
        {
            float rms_a;
            float rms_b;
            float rms_c;
            float phase_a_passed_resource;
            float phase_b_passed_resource;
            float phase_c_passed_resource;
            float phase_a_last_arc_lifetime;
            float phase_b_last_arc_lifetime;
            float phase_c_last_arc_lifetime;
            float phase_a_last_switch_off_current;
            float phase_b_last_switch_off_current;
            float phase_c_last_switch_off_current;
            float phase_a_last_switch_off_resource;
            float phase_b_last_switch_off_resource;
            float phase_c_last_switch_off_resource;
            float phase_a_operation_counter;
            float phase_b_operation_counter;
            float phase_c_operation_counter;
            float phase_a_mechanical_wear;
            float phase_b_mechanical_wear;
            float phase_c_mechanical_wear;
            float phase_a_electrical_wear;
            float phase_b_electrical_wear;
            float phase_c_electrical_wear;
            float phase_a_last_switch_electrical_wear;
            float phase_b_last_switch_electrical_wear;
            float phase_c_last_switch_electrical_wear;
            float full_off_time_a;
            float full_on_time_a;
            float full_off_time_b;
            float full_on_time_b;
            float full_off_time_c;
            float full_on_time_c;
            float temperature_pt;
            float short_circuit_on;
            float short_circuit_off;
            float operation_off_on;
            float operation_on_off;
            float own_time_off_state;
            float own_time_on_state;
            float full_time_off_state;
            float full_time_on_state;
        };
        float tf_values[TF_COUNT];
    };
};

/** @brief Runtime-данные второй версии модуля LCT1114. */
struct X2XLct1114_2 : X2XDeviceHeader
{
    static const uint8_t SP_COUNT = 20U;
    static const uint8_t ME_COUNT = 0U;
    static const uint8_t TF_COUNT = 45U;

    uint32_t digital_inputs;

    union
    {
        struct
        {
            float rms_a;
            float rms_b;
            float rms_c;
            float phase_a_passed_resource;
            float phase_b_passed_resource;
            float phase_c_passed_resource;
            float phase_a_last_arc_lifetime;
            float phase_b_last_arc_lifetime;
            float phase_c_last_arc_lifetime;
            float phase_a_last_switch_off_current;
            float phase_b_last_switch_off_current;
            float phase_c_last_switch_off_current;
            float phase_a_last_switch_off_resource;
            float phase_b_last_switch_off_resource;
            float phase_c_last_switch_off_resource;
            float phase_a_operation_counter;
            float phase_b_operation_counter;
            float phase_c_operation_counter;
            float phase_a_mechanical_wear;
            float phase_b_mechanical_wear;
            float phase_c_mechanical_wear;
            float phase_a_electrical_wear;
            float phase_b_electrical_wear;
            float phase_c_electrical_wear;
            float phase_a_last_switch_electrical_wear;
            float phase_b_last_switch_electrical_wear;
            float phase_c_last_switch_electrical_wear;
            float full_off_time_a;
            float full_on_time_a;
            float full_off_time_b;
            float full_on_time_b;
            float full_off_time_c;
            float full_on_time_c;
            float temperature_pt;
            float short_circuit_on;
            float short_circuit_off;
            float operation_off_on;
            float operation_on_off;
            float own_time_off_state;
            float own_time_on_state;
            float full_time_off_state;
            float full_time_on_state;
            float solenoid_current_on;
            float solenoid_current_off_1;
            float solenoid_current_off_2;
        };
        float tf_values[TF_COUNT];
    };

    /** Регистр 92 LCT2: текущее сырое состояние контактов. */
    uint16_t contacts_state_raw;

    /** Регистр 93 LCT2: предыдущее состояние контактов. */
    uint16_t previous_contacts_state;
};

/**
 * @brief Буфер последней считанной осциллограммы LCT.
 *
 * Буфер общий для X2X-сервиса, поскольку в каждый момент времени мастер
 * опрашивает только один модуль. Поле sequence увеличивается после полного
 * успешного чтения трёх фаз.
 */
struct X2XWaveformBuffer
{
    uint8_t valid;
    uint8_t owner_slave_address;
    uint16_t samples_per_phase;
    uint32_t sequence;
    int16_t phase[3][X2X_MAX_WAVEFORM_SAMPLES];
};

static_assert(sizeof(float) == 4U, "X2X requires 32-bit IEEE-754 float");
static_assert(sizeof(X2XLai1118::tf_values) == sizeof(float) * X2XLai1118::TF_COUNT,
              "LAI1118 channel array size mismatch");
static_assert(sizeof(X2XLct1114::tf_values) == sizeof(float) * X2XLct1114::TF_COUNT,
              "LCT1114 channel array size mismatch");
static_assert(sizeof(X2XLct1114_2::tf_values) == sizeof(float) * X2XLct1114_2::TF_COUNT,
              "LCT1114_2 channel array size mismatch");
