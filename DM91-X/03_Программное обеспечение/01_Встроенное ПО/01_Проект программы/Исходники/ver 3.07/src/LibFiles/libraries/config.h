#pragma once
#include <stdint.h>

extern const uint8_t slaveAddr;             // Адрес датчика
extern const float densitySetFlt;           // Плотность газа
extern const uint16_t incomingTimeOut;      // Через сколько мс перезагрузка
extern const uint8_t baudRate;              // Baud rate
extern const uint8_t parity;                // Parity
extern const uint16_t serialLow;            // Serial number low
extern const uint16_t serialHigh;           // Serial number high
extern const uint8_t hVersion;              // Версия железа
extern const uint8_t sVersion;              // Версия софта
extern const uint8_t modelDN;               // С влажностью 1; без — 0
extern const uint8_t getTempFromSHT;        // Источник температуры
extern const uint8_t liquefaction;          // Уровень сжижения
extern const float overdensity;             // Предельная плотность
extern const float warningLevel;            // Предупредительный уровень давления
extern const float alarmLevel;              // Аварийный уровень давления
extern const float offsetTemp;              // Температурный оффсет
extern const float offsetHumidity_b;        // Параметр b влажности
extern const float offsetHumidity_K;        // Параметр K влажности
extern const uint8_t tian_Pmin;             // Минимум давления, бар
extern const uint8_t tian_Pmax;             // Максимум давления, бар
extern const uint16_t tian_K;               // К-фактор давления
extern const uint16_t tian_Offset;          // Оффсет давления
extern const float tianTerm1;               // Коэффициент давления (расчётный)

extern const float kP;                      // PID: P
extern const float kI;                      // PID: I
extern const float kD;                      // PID: D

extern const int16_t tempSetPoint;          // Температурная уставка нагревателя
extern const uint8_t enableCPUheat;         // Активация нагревателя
