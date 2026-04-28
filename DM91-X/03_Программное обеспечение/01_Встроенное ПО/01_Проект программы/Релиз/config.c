#include <stdint.h>

// Адрес датчика 
const uint8_t slaveAddr = 3; 

 // Плотность газа
const float densitySetFlt = 6.176f;

 // через столько мс датчик перезагрузится при отсутствии входящих сообщений
const uint16_t incomingTimeOut = 5000; 

// Baud rate
const uint8_t baudRate = 3;

//Parity
const uint8_t parity = 0; 

// Serial number
const uint16_t serialLow = 0xAAAA;  
const uint16_t serialHigh = 0xAAAA;  

// Версия железа
const uint8_t hVersion = 101;

// Версия софта
const uint8_t sVersion = 102;  

// С влажностью 1; и без 0;
const uint8_t modelDN = 0;		

// считывать температуру с датчика влажностиа - 1. Считывать с датчика давления - 0
const uint8_t getTempFromSHT = 0;		

// Уровень плотности при котором происходит сжижение
const uint8_t liquefaction = 133;

// Предельный уровень плотности
const float overdensity =  80;

// Предупредительный уровень приведенного давления газа
const float warningLevel = 60;

// Аварийный уровень приведенного давления газа
const float alarmLevel = 50;

// Оффсет температуры
const float offsetTemp = 0;

// Коэффициент b кривой влажности 
const float offsetHumidity_b = 0; 
 
// Коэффициент k кривой влажности 
const float offsetHumidity_K = 0;

// Нижний предел давления в Барах датчика давления
const uint8_t tian_Pmin = 1;

// Верхний предел давления в Барах датчика давления
const uint8_t tian_Pmax = 20;

// коэффициент К датчика давления
const uint16_t tian_K = 13110;

// оффсет датчика давления
const uint16_t tian_Offset = 1638;

// расчет коэффициента датчика давления
//	tianTerm1 = (1.0 * (tian_Pmax+tian_Pmin))/(1.0*tian_K);
const float tianTerm1 = 0.001602;

// ---------------------------
// PID настройки нагревателя
// ---------------------------

const float kP	= 1.0;		// Коэффициент P.
const float kI	= 0.05;		// Коэффициент I.
const float kD	= 0.1;		// Коэффициент D.

// Уставка ниже которой включается нагреватель
const int16_t tempSetPoint = -20.0;

// Активировать нагреватель
const uint8_t enableCPUheat = 0;
