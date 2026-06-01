#pragma once

/**
 * @file app.hpp
 * @brief Интерфейс прикладного уровня прошивки DM-91x.
 *
 * @details
 * Заголовочный файл содержит публичные объявления функций прикладного уровня,
 * которые используются основным файлом прошивки и внутренними обработчиками
 * app.cpp. Основная логика реализована в app.cpp.
 *
 * Прошивка поддерживает независимые признаки исполнения:
 * - канал влажности: DM-91-0 без SHT31 или DM-91-H с SHT31;
 * - исполнение Arktic: наличие логики нагрева датчика давления и процессорной зоны.
 */

#include "project.hpp"

/** @name Команды и параметры датчика влажности SHT31 */
/** @{ */

/** @brief Команда чтения регистра статуса SHT31. */
#define SHT31_READ_STATUS       0xF32D

/** @brief Команда очистки регистра статуса SHT31. */
#define SHT31_CLEAR_STATUS      0x3041

/** @brief Команда программного сброса SHT31. */
#define SHT31_SOFT_RESET        0x30A2

/** @brief Команда аппаратного сброса SHT31. */
#define SHT31_HARD_RESET        0x0006

/** @brief Команда одиночного измерения SHT31 с коротким временем преобразования. */
#define SHT31_MEASUREMENT_FAST  0x2416U

/** @brief Команда одиночного измерения SHT31 с повышенной повторяемостью. */
#define SHT31_MEASUREMENT_SLOW  0x2400U

/** @brief Команда включения встроенного нагревателя SHT31. */
#define SHT31_HEAT_ON           0x306D

/** @brief Команда выключения встроенного нагревателя SHT31. */
#define SHT31_HEAT_OFF          0x3066

/** @brief Тайм-аут работы встроенного нагревателя SHT31, мс. */
#define SHT31_HEATER_TIMEOUT    180000UL

/** @} */

/**
 * @brief Инициализирует прикладной уровень прошивки.
 */
void setup(void);

/**
 * @brief Выполняет один проход основного цикла прикладной логики.
 */
void loop(void);

/**
 * @brief Инициализирует буферы фильтрации первыми измеренными значениями.
 *
 * @param pressVal_ Начальное значение давления.
 * @param tempVal_ Начальное значение температуры.
 * @param RHVal_ Начальное значение относительной влажности.
 */
void initArrays(float pressVal_, float tempVal_, float RHVal_);

/**
 * @brief Выполняет основную периодическую задачу измерений.
 */
void loop_50(void);

/**
 * @brief Запускает программную перезагрузку устройства через watchdog.
 */
void rebootDM(void);

/**
 * @brief Выполняет медленную периодическую задачу.
 */
void loop_500(void);

/**
 * @brief Обслуживает нагрев исполнения Arktic.
 */
void processHeater(void);

/**
 * @brief Выключает выходы нагрева и сбрасывает состояние регулятора.
 */
void heaterOff(void);

/**
 * @brief Контролирует наличие адресованных Modbus-запросов.
 */
void loop_5000(void);

/**
 * @brief Добавляет значения давления, температуры и влажности в буферы усреднения.
 *
 * @param press_ Давление.
 * @param temp_ Температура.
 * @param RH_ Относительная влажность.
 */
void avarageIt(float press_, float temp_, float RH_);

/**
 * @brief Выполняет сглаживание измеренных величин.
 */
void PIDfilter(void);

/**
 * @brief Обновляет трендовый буфер измеренных величин.
 */
void trendBuffer(void);

/**
 * @brief Загружает эксплуатационные настройки из EEPROM.
 */
void settings(void);

/**
 * @brief Обновляет битовое поле ошибок прибора.
 */
void checkError(void);

/**
 * @brief Проверяет завершение приёма пакета по отсутствию новых байтов.
 *
 * @param ch_ Номер канала.
 * @param length_ Текущее количество принятых байтов.
 *
 * @return 1, если пакет считается завершённым; иначе 0.
 */
uint8_t checkIncoming(uint8_t ch_, unsigned int length_);

/**
 * @brief Преобразует float в один из двух шестнадцатибитных Modbus-регистров.
 *
 * @param flt_ Значение float.
 * @param oreder_ Номер части float-значения.
 *
 * @return Значение Modbus-регистра.
 */
int convertFloat(float flt_, uint8_t oreder_);

/**
 * @brief Обрабатывает чтение и запись Modbus-регистров DM-91x.
 *
 * @param regs Буфер регистров ответа.
 * @param start_addr_ Начальный адрес запрошенного диапазона.
 * @param reg_count_ Количество регистров.
 * @param Value_ Значение при записи регистра.
 */
void modbusProceed(int16_t *regs, int16_t start_addr_, int16_t reg_count_, int16_t Value_);

/**
 * @brief Инициализирует Modbus RTU.
 */
void initCommunication(void);

/**
 * @brief Преобразует код скорости в значение UART baud rate.
 *
 * @param baudRate_ Код скорости.
 *
 * @return Скорость UART в бодах.
 */
unsigned long getBaud(uint8_t baudRate_);

/**
 * @brief Рассчитывает выход ПИД-регулятора нагрева.
 *
 * @param setpoint Заданная температура.
 * @param input Измеренная температура.
 *
 * @return Выход регулятора в диапазоне 0...255.
 */
uint8_t pid_control(float setpoint, float input);

/**
 * @brief Рассчитывает влажностные параметры.
 */
void getDewPoint(void);

/**
 * @brief Считывает давление и температуру с датчика давления/температуры.
 */
void getTian(void);

/**
 * @brief Считывает температуру и влажность с датчика SHT31.
 */
void getSHT(void);

/**
 * @brief Рассчитывает CRC-8 для блока данных SHT31.
 *
 * @param data Указатель на данные.
 * @param len Количество байтов.
 *
 * @return Рассчитанное значение CRC-8.
 */
uint8_t crc8(const uint8_t *data, uint8_t len);
