/**
 * @file app.cpp
 * @brief Прикладной уровень прошивки датчика плотности DM-91x.
 *
 * @details
 * Файл содержит основную логику прибора: загрузку настроек из EEPROM,
 * инициализацию интерфейсов, периодический опрос датчика давления/температуры,
 * опрос датчика влажности SHT31 для исполнения DM-91-H, расчёт плотности газа,
 * влажностных параметров, управление нагревом исполнения Arktic и обработку
 * карты регистров Modbus RTU.
 *
 * Датчик плотности DM-91x предназначен для непрерывного измерения абсолютного
 * давления газа или газовой смеси, температуры, влажности и преобразования
 * измеренных данных в функционально связанные величины.
 */

#include "app.hpp"
#include "dm91_eeprom.hpp"

#include "../Libs/IGAS_mb.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <avr/wdt.h>

/** @brief TWI-адрес датчика влажности SHT31. */
#define SHT31_ADDRESS   0x44U

/** @brief Код исполнения DM-91-H: прибор с каналом влажности. */
#define dm910H 1U

/** @brief Код исполнения DM-91-0: прибор без канала влажности. */
#define dm910 0U

/** @brief Обычное исполнение без логики нагрева Arktic. */
#define DM91_ARKTIC_DISABLED 0U

/** @brief Исполнение Arktic с нагревом датчика давления и процессорной зоны. */
#define DM91_ARKTIC_ENABLED  1U

/** @brief Экземпляр обработчика Modbus RTU по интерфейсу RS-485. */
IGAS_mb_485 IGAS_mb_X;

/**
 * @brief Время последнего принятого адресованного Modbus-запроса, мс.
 *
 * @details
 * Если адресованные запросы отсутствуют дольше incomingTimeOut, устройство
 * перезапускается watchdog-таймером.
 */
uint32_t incomingWDT = 0UL;

/** @brief Аппаратная версия устройства, выдаваемая через Modbus-регистр 110. */
uint16_t const hVersion = 5U;

/** @brief Версия встроенного ПО, выдаваемая через Modbus-регистр 111. */
uint16_t const sVersion = 15U;

/** @brief Modbus-адрес устройства. Загружается из EEPROM, регистр 100. */
uint8_t slaveAddr = 1U;

/** @brief Вывод управления направлением приёмопередатчика RS-485. */
uint8_t const RS_Pin = 2U;

/** @brief Маска вывода PD2 для прямого включения режима передачи RS-485. */
static const uint8_t transmitMode = static_cast<uint8_t>(1U << PD2);

/** @brief Вывод D3 управления нагревом процессорной зоны. */
uint8_t const Heat_CPU = 3U;

/** @brief Вывод D5 управления нагревателем датчика давления/температуры TIAN. */
uint8_t const Heat_Sensor = 5U;

/** @brief Вывод управления питанием датчика давления и температуры. */
uint8_t const Power_Sensor = 6U;

/** @brief Диагностическое состояние нагрева процессорной зоны. */
int16_t Heat_CPU_Current = 0;

/**
 * @brief Разрешение системы нагрева.
 *
 * @details
 * Параметр загружается из EEPROM и доступен через Modbus-регистр 158.
 * Нагрев фактически включается только если одновременно выбрано исполнение
 * Arktic и датчик давления/температуры TIAN отвечает без ошибки.
 */
uint8_t enableCPUheat = 0U;

/**
 * @brief Признак аппаратного исполнения Arktic.
 *
 * @details
 * Значение 0 соответствует обычному исполнению без нагрева. Значение 1
 * включает логику нагрева датчика давления/температуры TIAN и процессорной
 * зоны. Параметр хранится в EEPROM и доступен через Modbus-регистр 160.
 */
uint8_t arkticOption = DM91_ARKTIC_DISABLED;

/** @brief Резервный флаг потери связи с датчиком давления/температуры. */
byte lostTian = 0U;

/**
 * @brief Битовое поле ошибок прибора.
 *
 * @details
 * Значение выдаётся через Modbus-регистр 200. Биты используются для ошибок
 * давления, температуры, плотности, связи с датчиками и влажностных расчётов.
 */
int errorInt = 0;

/** @brief Код состояния связи с датчиком влажности SHT31. */
uint8_t shtSensorLost = 0U;

/** @brief Абсолютное давление, кПа. */
float pressure_kPa = 0.0f;

/** @brief Абсолютное давление, бар. */
float pressure_Bar = 0.0f;

/** @brief Абсолютное давление, МПа. */
float pressure_MPa = 0.0f;

/** @brief Абсолютное давление, Па. */
float pressure_Pa = 0.0f;

/** @brief Абсолютное давление, psi. */
float pressure_Psi = 0.0f;

/** @brief Абсолютное давление, Н/см². */
float pressure_Ncm2 = 0.0f;

/** @brief Отфильтрованное рабочее давление, кПа. */
float pressure = 0.0f;

/** @brief Отфильтрованная рабочая температура, °C. */
float temperature = 0.0f;

/** @brief Резервная температура канала влажности. */
float RH_Temp = 0.0f;

/** @brief Отфильтрованная относительная влажность, %. */
float RH_RH = 0.0f;

/** @brief Температура в градусах Цельсия. */
float temperatureC = 0.0f;

/** @brief Температура в Кельвинах. */
float temperatureK = 0.0f;

/** @brief Температура в градусах Фаренгейта. */
float temperatureF = 0.0f;

/** @brief Расчётная плотность газа. */
float gasDensityG = 0.0f;

/** @brief Порог плотности, связанный с областью сжижения газа. */
uint8_t liquefaction = 0U;

/** @brief Порог повышенной плотности газа. */
uint8_t overdensity = 0U;

/** @brief Давление, приведённое к 20 °C, бар. */
float pressure_Bar20 = 0.0f;

/** @brief Аварийный уровень давления, приведённого к 20 °C. */
uint8_t alarmLevel = 0U;

/** @brief Предупредительный уровень давления, приведённого к 20 °C. */
uint8_t warningLevel = 0U;

/** @brief Давление, приведённое к 20 °C, бар относительно атмосферного давления. */
float pressure_Bar20Rel = 0.0f;

/** @brief Давление, приведённое к 20 °C, МПа. */
float pressure_MPa20 = 0.0f;

/** @brief Давление, приведённое к 20 °C, МПа относительно атмосферного давления. */
float pressure_MPa20Rel = 0.0f;

/** @brief Давление насыщенного водяного пара. */
float Pws = 0.0f;

/** @brief Парциальное давление водяного пара. */
float Pw = 0.0f;

/** @brief Точка росы при давлении в резервуаре, °C. */
float dewPoint = 0.0f;

/** @brief Точка росы, пересчитанная к атмосферному давлению, °C. */
float dewPointAtm = 0.0f;

/** @brief Точка замерзания, пересчитанная к атмосферному давлению, °C. */
float frostPointAtm = 0.0f;

/** @brief Содержание влаги по объёму, ppmv. */
float PPMv = 0.0f;

/** @brief Содержание влаги по массе, ppmw. */
float PPMm = 0.0f;

/** @brief Относительная влажность, пересчитанная к атмосферному давлению. */
float RH_RH_Atm = 0.0f;

/** @brief Точка замерзания при давлении в резервуаре, °C. */
float frostPoint = 0.0f;

/** @brief Индекс текущей позиции в буферах скользящего среднего. */
uint8_t avarPosition = 0U;

/** @brief Сырое давление до фильтрации. Сохранено для диагностических регистров. */
float pressureRAW = 0.0f;

/** @brief Сырая температура до фильтрации. Сохранена для диагностических регистров. */
float temperatureRAW = 0.0f;

/** @brief Относительная влажность, измеренная SHT31, %. */
float SHT_RH_FL = 0.0f;

/** @brief Относительная влажность после калибровки. */
float RH_Tuned = 0.0f;

/** @brief Температура, измеренная SHT31, °C. */
float SHT_Temp_FL = 0.0f;

/** @brief Флаг ограничения отрицательной влажности. */
uint8_t negativeRHflag = 0U;

/** @brief Сырое значение влажности SHT31. */
uint16_t _rawHumidity = 0U;

/** @brief Сырое значение температуры SHT31. */
uint16_t _rawTemperature = 0U;

/** @brief Сырое значение давления датчика давления/температуры. */
uint16_t RAW_pressureTian = 0U;

/** @brief Сырое значение температуры датчика давления/температуры. */
uint16_t RAW_temperatureTian = 0U;

/** @brief Буфер скользящего среднего температуры. */
float tempAvar[12] = {0.0f};

/** @brief Буфер скользящего среднего влажности. */
float RHAvar[12] = {0.0f};

/** @brief Буфер скользящего среднего давления. */
float pressAvar[12] = {0.0f};

/** @brief Результаты скользящего среднего: давление, температура, влажность. */
float AVARAGED[4] = {0.0f};

/** @brief Результаты сглаживающего фильтра: давление, температура, влажность. */
float PIDFILTER[4] = {0.0f};

/** @brief Резервный уровень для фильтрации скачков. */
float topLevel = 0.0f;

/** @brief Резервный счётчик превышений верхнего порога фильтра. */
uint8_t STRIKE_H[3] = {0U};

/** @brief Резервный счётчик превышений нижнего порога фильтра. */
uint8_t STRIKE_L[3] = {0U};

/** @brief Индекс текущей позиции в трендовом буфере. */
uint8_t tail = 0U;

/** @brief Максимальный индекс трендового буфера. */
const uint8_t trendLimit = 5U;

/** @brief Трендовый буфер для давления, температуры и влажности. */
float TREND[3][trendLimit + 2U] = {{0.0f}};

/** @brief Резервное отфильтрованное значение температуры. */
float tempFiltered = 0.0f;

/** @brief Резервное отфильтрованное значение давления. */
float pressureFiltered = 0.0f;

/** @brief Коэффициенты мягкого сглаживания. */
float const SMOOTHfilter[] = {0.03f, 0.03f, 0.03f};

/** @brief Коэффициенты быстрого подтягивания фильтра при резком изменении сигнала. */
float const AGGRESSfilter[] = {0.5f, 0.5f, 0.5f};

/** @brief Резервное целочисленное давление датчика давления/температуры. */
int16_t tianPressure = 0;

/** @brief Резервная целочисленная температура датчика давления/температуры. */
int16_t tianTemp = 0;

/** @brief Давление, полученное из датчика давления/температуры, кПа. */
float pressureTian = 0.0f;

/** @brief Температура, полученная из датчика давления/температуры, °C. */
float temperatureTian = 0.0f;

/** @brief Младшая часть серийного номера устройства. */
int serialLow = 0;

/** @brief Старшая часть серийного номера устройства. */
int serialHigh = 0;

/** @brief Код скорости Modbus RTU, регистр 101. */
uint8_t baudRate = 0U;

/** @brief Код чётности Modbus RTU, регистр 102: 0 — без чётности, 1 — чётная. */
uint8_t parity = 0U;

/**
 * @brief Код исполнения устройства.
 *
 * @details
 * Значение 0 соответствует DM-91-0 без канала влажности.
 * Значение 1 соответствует DM-91-H с каналом влажности SHT31.
 */
uint8_t modelDN = dm910H;

/**
 * @brief Дополнительный газовый компонент бинарной смеси.
 *
 * @details
 * Прибор измеряет давление и температуру, а плотность рассчитывается с учётом
 * типа газа или газовой смеси. Параметр задаёт второй компонент смеси помимо
 * основного газа.
 */
uint8_t gasPartner = 0U;

/**
 * @brief Старый целочисленный параметр плотности газа.
 *
 * @details
 * Параметр сохранён в карте настроек и Modbus-регистрах. В текущих расчётах
 * используется densitySetFlt.
 */
unsigned int densitySet = 0U;

/**
 * @brief Рабочий коэффициент плотности газа в формате float.
 *
 * @details
 * Используется при расчёте плотности газа: gasDensityG = pressure_Bar20 * densitySetFlt.
 */
float densitySetFlt = 0.0f;

/** @brief Резервное время перезапуска. */
unsigned long resetWDtimer = 0UL;

/** @brief Резервный флаг watchdog. */
uint8_t watchDog = 0U;

/** @brief Резервный флаг сервисного режима. */
uint8_t serviceMode = 0U;

/** @brief Флаг запроса программной перезагрузки. */
uint8_t reboot = 0U;

/** @brief Флаг применения новых настроек после перезагрузки. */
uint8_t apply = 0U;

/** @brief Текущее время, мс. */
unsigned long currentTime = 0UL;

/** @brief Время последнего выполнения основной периодической задачи. */
unsigned long loop_50_start = 0UL;

/** @brief Период выполнения основного автомата измерений, мс. */
uint8_t const loop_50ms = 120U;

/** @brief Время последнего выполнения медленной периодической задачи. */
unsigned long loop_500_start = 0UL;

/** @brief Период медленной периодической задачи, мс. */
uint16_t const loop_500ms = 500U;

/** @brief Тайм-аут отсутствия адресованных Modbus-запросов до перезапуска, мс. */
uint16_t incomingTimeOut = 5000U;

/** @brief Сервисный флаг доступа к защищённым настройкам. */
uint8_t pinCode = 0U;

/** @brief Диагностический буфер ответа внешнего канала. */
uint8_t vipRespond[40] = {0U};

/** @brief Диагностический буфер байтов ответа датчика/внешнего канала. */
uint8_t vipMB_ByteReply[24] = {0U};

/** @brief Время начала ожидания между этапами опроса. */
unsigned long readDelay = 0UL;

/**
 * @brief Источник температуры для расчётов.
 *
 * @details
 * Параметр сохранён в карте настроек. В текущей активной логике расчётов
 * используется температура датчика давления/температуры.
 */
uint8_t getTempFromSHT = 0U;

/** @brief Объединение для преобразования float в байты Modbus-регистров. */
typedef union
{
	float val;
	uint8_t bytes[4];
} floatval;



/** @brief Режим сервисного моста. В текущей версии используется как сервисный флаг. */
uint8_t bridgeMode = 0U;

/** @brief Температурное смещение, применяемое при настройке прибора. */
float offsetTemp = 0.0f;

/** @brief Свободный член линейной калибровки влажности. */
float offsetHumidity_b = 0.0f;

/** @brief Коэффициент линейной калибровки влажности. */
float offsetHumidity_K = 0.0f;

/** @brief Нижняя граница диапазона датчика давления из заводского паспорта датчика. */
uint8_t tian_Pmin = 0U;

/** @brief Верхняя граница диапазона датчика давления из заводского паспорта датчика. */
uint8_t tian_Pmax = 0U;

/** @brief Масштабный коэффициент датчика давления из заводского паспорта датчика. */
uint16_t tian_K = 0U;

/** @brief Смещение нулевой точки датчика давления из заводского паспорта датчика. */
uint16_t tian_Offset = 0U;

/**
 * @brief Расчётный коэффициент преобразования сырого давления.
 *
 * @details
 * Коэффициент связан с заводскими параметрами датчика давления. В текущей
 * версии дополнительно задаётся фиксированным значением для сохранения
 * существующего поведения прошивки.
 */
float tianTerm1 = 0.0f;

/** @brief Резервный счётчик. */
uint8_t counterX = 0U;

/** @brief Резервный статус датчика влажности. */
uint16_t stat = 0U;

/** @brief Коэффициент пропорциональной составляющей регулятора. */
float kP = 1.0f;

/** @brief Коэффициент интегральной составляющей регулятора. */
float kI = 0.05f;

/** @brief Коэффициент дифференциальной составляющей регулятора. */
float kD = 0.1f;

/** @brief Текущая ошибка регулятора. */
float curr_error = 0.0f;

/** @brief Предыдущая ошибка регулятора. */
float prev_error = 0.0f;

/** @brief Интегральная составляющая регулятора. */
float integral = 0.0f;

/** @brief Минимальное значение выхода регулятора. */
#define MIN_OUTPUT	0

/** @brief Максимальное значение выхода регулятора. */
#define MAX_OUTPUT	255

/** @brief Уставка температуры регулятора нагрева датчика давления, °C. */
float tempSetPoint = 0.0f;

/** @brief Текущее выходное значение регулятора нагрева датчика давления, 0...255. */
uint8_t PWM_SP = 0U;

/** @brief Флаг потери связи с датчиком давления/температуры TIAN. */
uint8_t pressGaugeLost = 0U;

/** @brief Период окна медленного программного ШИМ нагревателя датчика, мс. */
static const unsigned long HEAT_SENSOR_PWM_PERIOD_MS = 5000UL;

/** @brief Температура включения нагрева процессорной зоны, °C. */
static const float HEAT_CPU_ON_TEMPERATURE = 0.0f;

/** @brief Температура выключения нагрева процессорной зоны, °C. */
static const float HEAT_CPU_OFF_TEMPERATURE = 2.0f;

/** @brief Начало текущего окна программного ШИМ нагревателя датчика, мс. */
unsigned long heatSensorPwmWindowStart = 0UL;

/** @brief Текущее дискретное состояние нагрева процессорной зоны. */
uint8_t heatCpuState = 0U;

/**
 * @brief Рассчитывает выход ПИД-регулятора нагрева.
 *
 * @param setpoint Заданная температура.
 * @param input Текущая измеренная температура.
 *
 * @return Выходное значение регулятора в диапазоне 0...255.
 */
uint8_t pid_control(float setpoint, float input)
{
	curr_error = setpoint - input;

	float proportional = kP * curr_error;

	integral += kI * curr_error;

	if (integral > MAX_OUTPUT){
		integral = MAX_OUTPUT;
		} else if (integral < MIN_OUTPUT){
		integral = MIN_OUTPUT;
	};

	float derivative = kD * (curr_error - prev_error);
	prev_error = curr_error;

	float output = proportional + integral + derivative;

	if (output > MAX_OUTPUT){
		output = MAX_OUTPUT;
		} else if (output < MIN_OUTPUT){
		output = MIN_OUTPUT;
	};

	return (uint8_t) output;
}

/**
 * @brief Выключает все выходы нагрева и сбрасывает состояние регулятора.
 *
 * @details
 * Используется при выключенном исполнении Arktic, запрете нагрева или потере
 * связи с датчиком давления/температуры. Сброс интегральной составляющей
 * предотвращает накопление мощности нагрева при недостоверной обратной связи.
 */
void heaterOff(void)
{
	PWM_SP = 0U;
	digitalWrite(Heat_Sensor, LOW);
	digitalWrite(Heat_CPU, LOW);
	heatCpuState = 0U;
	Heat_CPU_Current = 0;
	integral = 0.0f;
	prev_error = 0.0f;
	curr_error = 0.0f;
	heatSensorPwmWindowStart = currentTime;
}

/**
 * @brief Обслуживает нагрев исполнения Arktic.
 *
 * @details
 * Нагрев доступен только при выбранном исполнении Arktic, разрешении нагрева
 * и исправной связи с датчиком давления/температуры TIAN. Нагреватель датчика
 * давления управляется медленным программным ШИМ с периодом пять секунд.
 * Мощность ШИМ рассчитывается ПИД-регулятором по температуре temperatureTian.
 *
 * Нагрев процессорной зоны управляется дискретно по той же температуре TIAN:
 * включается ниже 0 °C и выключается выше 2 °C. Это упрощённая защитная
 * логика для исполнения Arktic без использования отдельного датчика
 * температуры микроконтроллера.
 */
void processHeater(void)
{
	if ((arkticOption != DM91_ARKTIC_ENABLED) || (enableCPUheat == 0U) || (pressGaugeLost != 0U))
	{
		heaterOff();
		return;
	}

	PWM_SP = pid_control(tempSetPoint, temperatureTian);

	if ((currentTime - heatSensorPwmWindowStart) >= HEAT_SENSOR_PWM_PERIOD_MS)
	{
		heatSensorPwmWindowStart = currentTime;
	}

	const unsigned long elapsed = currentTime - heatSensorPwmWindowStart;
	const unsigned long onTime = (static_cast<unsigned long>(PWM_SP) * HEAT_SENSOR_PWM_PERIOD_MS) / 255UL;

	if ((PWM_SP > 0U) && (elapsed < onTime))
	{
		digitalWrite(Heat_Sensor, HIGH);
	}
	else
	{
		digitalWrite(Heat_Sensor, LOW);
	}

	if (temperatureTian < HEAT_CPU_ON_TEMPERATURE)
	{
		heatCpuState = 1U;
	}
	else if (temperatureTian > HEAT_CPU_OFF_TEMPERATURE)
	{
		heatCpuState = 0U;
	}

	digitalWrite(Heat_CPU, (heatCpuState != 0U) ? HIGH : LOW);
	Heat_CPU_Current = static_cast<int16_t>(heatCpuState);
}

/** @brief Диагностическое значение внешнего температурного канала. */
float DS18B20_Temp = 0.0f;

/**
 * @brief Инициализирует прикладной уровень прошивки.
 *
 * @details
 * Отключает и заново включает watchdog, загружает настройки из EEPROM,
 * настраивает управляющие выводы, инициализирует Modbus RTU и TWI, затем
 * формирует стартовую посылку с адресом устройства.
 */
void setup()
{
	





	wdt_disable();
	wdt_enable(WDTO_2S);
	



	settings();


	pinMode(RS_Pin, OUTPUT);
	digitalWrite(RS_Pin, LOW);
	pinMode(Heat_CPU, OUTPUT);
	pinMode(Heat_Sensor, OUTPUT);
	pinMode(Power_Sensor, OUTPUT);
	digitalWrite(Heat_CPU, LOW);
	digitalWrite(Heat_Sensor, LOW);
	digitalWrite(Power_Sensor, LOW);

	initCommunication();
	delay(50);

	
	UCSR0A = UCSR0A | (1 << TXC0);
	uint8_t oldSREG = SREG;
	cli();
	PORTD = PORTD | transmitMode;
	SREG = oldSREG;
	
	Serial.write(slaveAddr);
	Serial.write(slaveAddr);
	Serial.write(slaveAddr);
	Serial.write(slaveAddr);
	
	
	
	
	hal::twi_init(100000UL);

	incomingWDT = millis();
	
	wdt_reset();
}
/** @brief Флаг первого заполнения буферов фильтрации. */
uint8_t VIP_firstRead = 0U;

/** @brief Резервный счётчик. */
uint8_t i = 1U;
/** @brief Флаг запуска Modbus-обработки после стартовой задержки. */
uint8_t startMB = 0U;
/** @brief Диагностический счётчик heartbeat. */
uint16_t watchdogQnt = 0U;

/**
 * @brief Выполняет один проход основного цикла прошивки.
 *
 * @details
 * Обновляет системное время, запускает периодические задачи после стартовой
 * задержки, обслуживает Modbus RTU и сбрасывает watchdog.
 */
void loop()
{

	currentTime = millis();

	if (bridgeMode == 1)
	{
		bridgeMode = 2;
		wdt_disable();
	}

	
	if (currentTime > 3000 && startMB == 0)
	{
		startMB = 1;
	}

	if (startMB)
	{
		loop_5000();
		loop_500();
		loop_50();
		IGAS_mb_X.update();
	}
	
	wdt_reset();
	
}


/**
 * @brief Состояния автомата опроса датчиков и расчёта параметров.
 */
enum
{
	dmReady,
	powerUpTian,
	waitPowerUpTian,
	sendMsg,
	waitTransfer,
	waitReply,
	receiveMsg,
	requestRH,
	receiveRH,
	timeEqualization,
	averaging,
	trending,
	noiseCancel,
	gapCancel,
	calculate,
	pollDelay,
};


/** @brief Текущее состояние автомата измерений. */
uint8_t sensorStep = dmReady;


/** @brief Количество последовательных ошибок связи с датчиком давления. */
uint8_t lostCntPressure = 0;
/** @brief Общий счётчик ошибок связи с датчиком давления с момента включения. */
unsigned int totalLostPressureCnt = 0;

/** @brief Количество последовательных ошибок связи с датчиком влажности. */
uint8_t lostCntRH = 0;
/** @brief Общий счётчик ошибок связи с датчиком влажности с момента включения. */
unsigned int totalLostRHCnt = 0;

/** @brief Количество байтов в диагностическом ответе. */
byte vipReplyQnt = 0;


/**
 * @brief Выполняет один шаг автомата измерений DM-91x.
 *
 * @details
 * Автомат включает питание датчика давления/температуры, выполняет TWI-опрос,
 * при исполнении DM-91-H опрашивает датчик влажности SHT31, затем обновляет
 * фильтры и рассчитывает выходные величины для Modbus-регистров.
 */
void mainDM()
{
	switch (sensorStep)
	{
		case powerUpTian:
		{
			digitalWrite(Power_Sensor, HIGH);
			sensorStep = waitPowerUpTian;
		}
		break;
		
		case waitPowerUpTian:
		{
			sensorStep = sendMsg;
		}
		break;
		
		
		case sendMsg:
		{
			getTian();
			sensorStep = waitTransfer;
		}
		break;
		
		
		case waitTransfer:
		{
			sensorStep = waitReply;
			readDelay = currentTime;
		}
		break;
		
		case waitReply:
		{
			if (currentTime - readDelay > 100)
			{
				if (modelDN == dm910H)
				{
					sensorStep = requestRH;
				}
				
				else
				{
					readDelay = currentTime;
					sensorStep = averaging;
				}
			}
		}
		break;
		

		
		case requestRH:
		{
			getSHT();
			sensorStep = averaging;
			
		}
		break;


		case timeEqualization:
		{
			if (currentTime - readDelay > 50)
			sensorStep = averaging;
		}
		break;

		case receiveRH:
		{
			
			sensorStep = averaging;
		}
		break;
		
		
		case averaging:
		{
			digitalWrite(Power_Sensor, LOW);
			if (pressGaugeLost == 0 && lostCntPressure == 0)
			{
				
				
				
				if (VIP_firstRead == 0)
				initArrays(pressureTian, temperatureTian, SHT_RH_FL);
			}
			

			checkError();
			avarageIt(pressureTian, temperatureTian, SHT_RH_FL);
			
			sensorStep = trending;
		}
		break;
		
		
		case trending:
		{
			trendBuffer();
			sensorStep = noiseCancel;
		}

		
		break;
		
		case noiseCancel:
		{
			PIDfilter();
			pressure = PIDFILTER[0];
			temperature = PIDFILTER[1];
			RH_RH = PIDFILTER[2];

			sensorStep = calculate;
		}
		break;
		
		
		
		case calculate:
		{
			pressure_Bar = pressure * 0.01;
			pressure_MPa = pressure_Bar *0.1;
			pressure_Pa = 1000.0 * pressure;
			pressure_kPa = pressure;
			pressure_Psi = pressure * 0.145038;
			pressure_Ncm2 = pressure * 0.1;
			
			temperatureC = temperature;
			temperatureK = temperature+273.15;
			temperatureF = temperature*1.8+32.0;
			
			float buffer;
			buffer = 293.15/ temperatureK;

			
			pressure_Bar20 = pressure_Bar * buffer;
			pressure_Bar20Rel = pressure_Bar20 - 1.0;
			pressure_MPa20 = pressure_Bar20 * 0.1;
			pressure_MPa20Rel = pressure_MPa20 - 0.1;
			
			gasDensityG = pressure_Bar20 * densitySetFlt;
			
			if (modelDN == dm910H)
			getDewPoint();
			
			sensorStep = dmReady;
		}
		break;
		
		
		case pollDelay:
		
		break;
		
		default:
		
		break;
		
		
	}

}



/**
 * @brief Инициализирует буферы фильтрации первыми измеренными значениями.
 *
 * @param pressVal_ Начальное значение давления.
 * @param tempVal_ Начальное значение температуры.
 * @param RHVal_ Начальное значение относительной влажности.
 */
void initArrays(float pressVal_, float tempVal_, float RHVal_)
{
	uint8_t i;
	VIP_firstRead = 1;
	for (i=0; i<=4; i++)
	{
		pressAvar[i] = pressVal_;
		tempAvar[i] = tempVal_;
		RHAvar[i] = RHVal_;
	}
	
	for (i=0; i<=trendLimit;i++ )
	TREND[0][i] = pressVal_;

	for (i=0; i<=trendLimit;i++ )
	TREND[1][i] = tempVal_;
	
	for (i=0; i<=trendLimit;i++ )
	TREND[2][i] = RHVal_;
	
	PIDFILTER[0] = pressVal_;
	PIDFILTER[1] = tempVal_;
	PIDFILTER[2] = RHVal_;
	
}

/**
 * @brief Обновляет трендовый буфер давления, температуры и влажности.
 */
void trendBuffer()
{
	tail++;
	if (tail>trendLimit)
	tail = 0;

	for (uint8_t i = 0; i<=2; i++)
	TREND[i][tail] = AVARAGED[i];
}


/**
 * @brief Обновляет битовое поле ошибок прибора.
 *
 * @details
 * Проверяются диапазоны давления, температуры, плотности, приведённого давления
 * и состояние влажностных расчётов. Результат доступен через Modbus-регистр 200.
 */
void checkError()
{
	
	if (!pressGaugeLost)
	{
		if (pressureRAW < 0)
		errorInt |= 1;

		
		if (pressureRAW > 5000)
		errorInt |= 1 << 1;
		
		
		if (pressureRAW > 10000 || pressureRAW < -100)
		errorInt |= 1 << 2;

		
		if (temperatureRAW < (-60))
		errorInt |= 1 << 3;
		
		if (temperatureRAW > 80)
		errorInt |= 1 << 4;
		
		if (gasDensityG > liquefaction)
		errorInt |= 1 << 6;
		
		if (gasDensityG > overdensity)
		errorInt |= 1 << 7;
		
		if (pressure_Bar20 < alarmLevel)
		errorInt |= 1 << 11;
		
		if ((pressure_Bar20 > alarmLevel) && (pressure_Bar20 < warningLevel))
		errorInt |= 1 << 12;
	}
	



	
	
	
	
	if (negativeRHflag)
	errorInt |= 1 << 13;
	
	
}


/**
 * @brief Выполняет сглаживание давления, температуры и влажности.
 *
 * @details
 * При малом изменении сигнала используется мягкий коэффициент сглаживания,
 * при резком изменении — более агрессивный коэффициент.
 */
void PIDfilter()
{
	float noiseMax = -1000.0;
	float noiseMin = 100000.0;
	float powerShift;

	for (uint8_t i = 0; i <= 2; i++)
	{
		noiseMax = -1000.0;
		noiseMin = 100000.0;
		for (uint8_t j = 0; j < 5; j++)
		{
			if (TREND[i][j] > noiseMax)
			noiseMax = TREND[i][j];
			if (TREND[i][j] < noiseMin)
			noiseMin = TREND[i][j];
		}

		noiseMin = noiseMin * 0.99;
		noiseMax = noiseMax * 1.01;


		if  (PIDFILTER[i] > noiseMin &&  noiseMax > PIDFILTER[i])
		powerShift = SMOOTHfilter[i];
		else
		powerShift = AGGRESSfilter[i];

		PIDFILTER[i] += powerShift * (1.0 * AVARAGED[i] - PIDFILTER[i]);
	}

}


/**
 * @brief Выполняет основную периодическую задачу измерений.
 */
void loop_50()
{
	if (currentTime - loop_50_start > loop_50ms)
	{
		loop_50_start = currentTime;
		mainDM();
		
		
		if (reboot)
		rebootDM();
		

		
	}
}


/**
 * @brief Запускает программную перезагрузку через watchdog.
 */
void rebootDM()
{
	wdt_disable();
	wdt_enable(WDTO_15MS);
	for (;;)
	{
	}
}

/**
 * @brief Контролирует наличие адресованных Modbus-запросов.
 *
 * @details
 * Если устройство дольше incomingTimeOut не получает адресованные запросы,
 * запускается перезагрузка через watchdog.
 */
void loop_5000()
{
	if (currentTime - incomingWDT > incomingTimeOut)
	{
		
		wdt_enable(WDTO_15MS);
		for (;;)
		{
		}
	}
}


/**
 * @brief Выполняет медленную периодическую задачу.
 *
 * @details
 * Запускает новый цикл измерения, если автомат находится в состоянии dmReady,
 * увеличивает диагностический счётчик heartbeat и обслуживает нагрев исполнения
 * Arktic.
 */
void loop_500()
{
	if (currentTime - loop_500_start > loop_500ms)
	{
		loop_500_start = currentTime;
		if (sensorStep == dmReady)
		sensorStep = powerUpTian;
		
		watchdogQnt++;
		processHeater();
	}
}

/** @brief Буфер ответа датчика давления/температуры. */
uint8_t BufWT[10];
/** @brief Буфер ответа датчика влажности SHT31. */
uint8_t BufSHT[10];
/** @brief TWI-адрес датчика давления/температуры. */
uint8_t addrWT = 0x28;



/**
 * @brief Считывает давление и температуру с датчика давления/температуры.
 *
 * @details
 * По TWI отправляется команда измерения, затем считываются четыре байта ответа.
 * Из ответа формируются сырые значения давления и температуры, после чего
 * рассчитываются pressureTian и temperatureTian.
 */
void getTian()
{
	const uint8_t command = 0x50U;

	if (!hal::twi_write(addrWT, &command, 1U))
	{
		pressGaugeLost = 1U;
		lostCntPressure++;
		totalLostPressureCnt++;
		return;
	}

	if (!hal::twi_read(addrWT, BufWT, 4U))
	{
		pressGaugeLost = 1U;
		lostCntPressure++;
		totalLostPressureCnt++;
		return;
	}

	pressGaugeLost = 0U;
	lostCntPressure = 0U;

	RAW_pressureTian = (static_cast<uint16_t>(BufWT[0] & 0x3FU) << 8) | BufWT[1];
	pressureTian = 100.0f + 100.0f * (((RAW_pressureTian - 1638) * 0.001602f) - 1.0f);

	RAW_temperatureTian = (static_cast<uint16_t>(BufWT[2]) << 3) | ((BufWT[3] >> 5) & 0x07U);
	temperatureTian = 200.0f * RAW_temperatureTian / 2047.0f - 50.0f;

	delayMicroseconds(150);
}


/**
 * @brief Считывает температуру и относительную влажность с датчика SHT31.
 *
 * @details
 * Отправляет команду одиночного измерения, считывает шесть байт ответа,
 * проверяет CRC температуры и влажности, затем рассчитывает SHT_Temp_FL и
 * SHT_RH_FL.
 */
void getSHT()
{
	const uint8_t command[2] =
	{
		static_cast<uint8_t>(SHT31_MEASUREMENT_SLOW >> 8),
		static_cast<uint8_t>(SHT31_MEASUREMENT_SLOW & 0xFFU)
	};

	if (!hal::twi_write(SHT31_ADDRESS, command, 2U))
	{
		shtSensorLost = 1U;
		lostCntRH++;
		totalLostRHCnt++;
		return;
	}

	delay(20);

	if (!hal::twi_read(SHT31_ADDRESS, BufSHT, 6U))
	{
		shtSensorLost = 1U;
		lostCntRH++;
		totalLostRHCnt++;
		return;
	}

	if ((BufSHT[2] != crc8(BufSHT, 2U)) || (BufSHT[5] != crc8(BufSHT + 3U, 2U)))
	{
		shtSensorLost = 88U;
		lostCntRH++;
		totalLostRHCnt++;
		return;
	}

	shtSensorLost = 0U;
	lostCntRH = 0U;

	_rawTemperature = (static_cast<uint16_t>(BufSHT[0]) << 8) + BufSHT[1];
	_rawHumidity = (static_cast<uint16_t>(BufSHT[3]) << 8) + BufSHT[4];

	SHT_Temp_FL = (_rawTemperature * 175.0f / 65535.0f) - 45.0f;
	SHT_RH_FL = (_rawHumidity * 100.0f / 65535.0f);
}

/**
 * @brief Рассчитывает CRC-8 для блока данных SHT31.
 *
 * @param data Указатель на массив данных.
 * @param len Количество байтов.
 *
 * @return Рассчитанное значение CRC-8.
 */
uint8_t crc8(const uint8_t *data, uint8_t len)
{
	const uint8_t POLY(0x31);
	uint8_t crc(0xFF);

	for (uint8_t j = len; j; --j)
	{
		crc ^= *data++;

		for (uint8_t i = 8; i; --i)
		{
			crc = (crc & 0x80) ? (crc << 1) ^ POLY : (crc << 1);
		}
	}
	return crc;
}

/**
 * @brief Добавляет измеренные значения в буферы скользящего среднего.
 *
 * @param press_ Давление.
 * @param temp_ Температура.
 * @param RH_ Относительная влажность.
 */
void avarageIt(float press_, float temp_,float RH_)
{
	float bufferP_ = 0;
	float bufferT_ = 0;
	float bufferRH_ = 0;
	
	pressAvar[avarPosition] = press_;
	tempAvar[avarPosition] = temp_;
	RHAvar[avarPosition] = RH_;
	avarPosition++;
	
	if (avarPosition > 4)
	avarPosition = 0;
	
	for (uint8_t i=0; i<=4; i++)
	{
		bufferP_ +=  pressAvar[i];
		bufferT_ +=  tempAvar[i];
		bufferRH_ += RHAvar[i];
		
	}
	
	AVARAGED[0] = bufferP_*0.2;
	AVARAGED[1] = bufferT_*0.2;
	AVARAGED[2] = bufferRH_*0.2;
}


/**
 * @brief Инициализирует Modbus RTU.
 *
 * @details
 * Настраивает скорость, формат кадра, обработчик карты регистров и адрес
 * Modbus RTU.
 */
void initCommunication()
{
	unsigned int baud_;
	uint8_t parity_ = 0;
	
	baud_ =  getBaud(baudRate);
	
	if (parity == 1)
	parity_ = SERIAL_8E1;
	else
	parity_ = SERIAL_8N1;
	
	IGAS_mb_X.initMB(baud_, parity_, 1001);
	IGAS_mb_X.setDelegate(modbusProceed);
	IGAS_mb_X.slave = slaveAddr;
	

	
}


/**
 * @brief Преобразует код скорости из EEPROM/Modbus в скорость UART.
 *
 * @param baudRate_ Код скорости.
 *
 * @return Скорость UART в бодах.
 */
unsigned long getBaud(uint8_t baudRate_)
{
	unsigned long retBaud = 0;
	
	switch (baudRate_)
	{
		case 1:
		retBaud = 2400;
		break;
		
		case 2:
		retBaud = 4800;
		break;
		
		case 3:
		retBaud = 9600;
		break;
		
		case 4:
		retBaud = 14400;
		break;
		
		case 5:
		retBaud = 19200;
		break;
		
		case 6:
		retBaud = 38400;
		break;
		
		case 7:
		retBaud = 56000;
		break;
		
		default:
		retBaud = 9600;
		break;
	}
	
	return retBaud;
}


/**
 * @brief Обрабатывает чтение и запись Modbus-регистров DM-91x.
 *
 * @param regs Буфер регистров ответа.
 * @param start_addr_ Начальный адрес запрошенного диапазона.
 * @param reg_count_ Количество регистров.
 * @param Value_ Значение при записи регистра.
 *
 * @details
 * Функция содержит карту рабочих, диагностических и настроечных регистров.
 * Часть настроечных регистров сохраняется в EEPROM. Защищённые настройки
 * требуют предварительной установки сервисного PIN-кода через регистр 800.
 */
void modbusProceed(int16_t *regs, int16_t start_addr_, int16_t reg_count_, int16_t Value_)
{
	int registerAddr_ = start_addr_;
	uint8_t writeReg = 0;
	if (IGAS_mb_X.func == FC_WRITE_REGS || IGAS_mb_X.func == FC_WRITE_REG)
	writeReg = 1;
	
	incomingWDT = currentTime;
	
	for (uint8_t i = 0; i <= reg_count_ - 1; i++)
	{
		switch (registerAddr_)
		{
			
			case 0:
			
			regs[i] = convertFloat(pressure_Bar, 0);

			break;
			
			
			case 1:
			regs[i] = convertFloat(pressure_Bar, 1);

			break;
			
			case 2:

			regs[i] = convertFloat(pressure_MPa, 0);

			break;
			
			case 3:
			regs[i] = convertFloat(pressure_MPa, 1);

			break;
			
			case 4:
			
			regs[i] = convertFloat(pressure_Pa, 0);

			break;
			
			
			case 5:
			
			regs[i] = convertFloat(pressure_Pa, 1);

			break;
			
			case 6:
			
			regs[i] = convertFloat(pressure_kPa, 0);

			break;
			
			case 7:

			regs[i] = convertFloat(pressure_kPa, 1);

			break;
			
			
			case 8:
			

			regs[i] = convertFloat(pressure_Psi, 0);

			break;
			
			
			case 9:

			regs[i] = convertFloat(pressure_Psi, 1);

			break;
			
			
			case 10:
			
			regs[i] = convertFloat(pressure_Ncm2, 0);

			break;
			
			
			case 11:

			regs[i] = convertFloat(pressure_Ncm2, 1);

			break;
			
			
			case 12:
			
			regs[i] = convertFloat(temperatureC, 0);
			
			break;
			
			
			case 13:

			regs[i] = convertFloat(temperatureC, 1);

			break;
			
			case 14:
			
			regs[i] = convertFloat(temperatureK, 0);

			break;
			
			
			case 15:

			regs[i] = convertFloat(temperatureK, 1);

			break;
			
			case 16:
			

			regs[i] = convertFloat(temperatureF, 0);

			break;
			
			
			case 17:

			regs[i] = convertFloat(temperatureF, 1);

			break;
			
			case 18:
			

			regs[i] = convertFloat(gasDensityG, 0);

			break;
			
			
			case 19:

			regs[i] = convertFloat(gasDensityG, 1);

			break;
			
			
			case 20:
			

			regs[i] = convertFloat(gasDensityG, 0);

			break;
			
			
			case 21:

			regs[i] = convertFloat(gasDensityG, 1);

			break;


			case 22:
			

			regs[i] = convertFloat(pressure_Bar20, 0);

			break;
			
			
			case 23:

			regs[i] = convertFloat(pressure_Bar20, 1);

			break;
			
			case 24:

			regs[i] = convertFloat(frostPointAtm, 0);

			break;
			
			
			case 25:

			regs[i] = convertFloat(frostPointAtm, 1);

			break;
			
			
			case 26:

			regs[i] = convertFloat(dewPointAtm, 0);

			break;
			
			
			case 27:

			regs[i] = convertFloat(dewPointAtm, 1);

			break;
			
			
			case 28:

			regs[i] = convertFloat(frostPoint, 0);

			break;
			
			
			case 29:

			regs[i] = convertFloat(frostPoint, 1);

			break;
			
			
			
			case 30:

			regs[i] = convertFloat(dewPoint, 0);

			break;
			
			
			case 31:

			regs[i] = convertFloat(dewPoint, 1);

			break;
			
			
			


			case 40:

			regs[i] = convertFloat(PPMv, 0);

			break;
			
			case 41:

			regs[i] = convertFloat(PPMv, 1);

			break;
			
			case 42:

			regs[i] = convertFloat(PPMm, 0);

			break;
			
			case 43:

			regs[i] = convertFloat(PPMm, 1);

			break;
			
			
			
			case 48:

			regs[i] = convertFloat(RH_Tuned, 0);

			break;
			
			case 49:

			regs[i] = convertFloat(RH_Tuned, 1);

			break;
			
			
			

			case 58:
			
			regs[i] = convertFloat(pressure_Bar20Rel, 0);

			break;
			
			
			case 59:

			regs[i] = convertFloat(pressure_Bar20Rel, 1);

			break;
			
			case 60:
			

			regs[i] = convertFloat(pressure_MPa20, 0);

			break;
			
			
			case 61:

			regs[i] = convertFloat(pressure_MPa20, 1);

			break;
			
			case 62:

			regs[i] = convertFloat(pressure_MPa20Rel, 0);

			break;
			
			
			case 63:

			regs[i] = convertFloat(pressure_MPa20Rel, 1);

			break;
			
			
			case 70:
			
			regs[i] = convertFloat(pressureRAW,0);
			
			break;
			
			case 71:
			
			regs[i] = convertFloat(pressureRAW,1);
			
			break;
			
			case 72:
			
			regs[i] = convertFloat(temperatureRAW,0);
			
			break;
			
			case 73:
			
			regs[i] = convertFloat(temperatureRAW,1);
			
			break;
			
			case 74:
			
			regs[i] = convertFloat(pressure,0);
			
			break;
			
			case 75:
			
			regs[i] = convertFloat(pressure,1);
			
			break;
			
			case 76:
			
			regs[i] = convertFloat(temperature,0);
			
			break;
			
			case 77:
			
			regs[i] = convertFloat(temperature,1);
			
			break;
			
			
			case 78:
			
			regs[i] =  lostCntPressure;

			break;
			
			
			case 79:
			
			regs[i] =  totalLostPressureCnt;

			break;
			
			case 80:
			
			regs[i] = pressGaugeLost;

			break;
			
			
			case 81:
			
			regs[i] = lostCntRH;

			break;
			
			case 82:
			
			regs[i] =  totalLostRHCnt;

			break;
			
			case 83:
			
			regs[i] =  shtSensorLost;

			break;


			case 99:
			regs[i] =  Value_;
			break;
			

			case 100:

			if (writeReg)
			{
				slaveAddr = Value_;
				IGAS_mb_X.slave = slaveAddr;
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_SLAVE_ADDRESS, slaveAddr);
			}
			
			regs[i] = slaveAddr;

			break;
			
			
			case 101:

			if (writeReg)
			{
				baudRate = Value_;
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_BAUD_RATE, baudRate);
			}
			regs[i] = baudRate;

			break;
			
			
			case 102:

			if (writeReg)
			{
				parity = Value_;
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_PARITY, parity);
			}

			regs[i] = parity;

			break;
			
			case 104:

			if (writeReg)
			{
				densitySet = Value_;
				dm91_eeprom::saveInt(dm91_eeprom::ADDR_DENSITY_SET, densitySet);
			}
			
			regs[i] = densitySet;

			break;
			
			case 105:

			if (writeReg)
			{
				gasPartner = Value_;
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_GAS_PARTNER, gasPartner);
			}
			
			regs[i] = gasPartner;

			break;
			
			
			case 106:

			if (writeReg && pinCode)
			{
				serialHigh = Value_;
				dm91_eeprom::saveInt(dm91_eeprom::ADDR_SERIAL_HIGH, serialHigh);
			}
			
			regs[i] = serialHigh;

			break;
			
			case 107:

			if (writeReg && pinCode)
			{
				serialLow = Value_;
				dm91_eeprom::saveInt(dm91_eeprom::ADDR_SERIAL_LOW, serialLow);
			}
			regs[i] = serialLow;

			break;
			

			case 110:
			regs[i] = hVersion;

			break;

			case 111:
			regs[i] = sVersion;
			
			break;
			
			
			case 112:

			if (writeReg && pinCode)
			{
				modelDN = Value_;
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_MODEL, modelDN);
				
				
			}
			regs[i] = modelDN;

			break;
			
			case 113:

			if (writeReg && pinCode)
			{
				getTempFromSHT = Value_;
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_TEMP_SOURCE, getTempFromSHT);
				
				
			}
			regs[i] = getTempFromSHT;

			break;
			
			
			
			case 130:
			
			if (writeReg && pinCode)
			{
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_DENSITY_FLOAT + 2U, (Value_ >> 8));
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_DENSITY_FLOAT + 3U, (Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(densitySetFlt, 0);
			
			break;
			
			case 131:
			
			if (writeReg && pinCode)
			{
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_DENSITY_FLOAT, (Value_ >> 8));
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_DENSITY_FLOAT + 1U, (Value_ & 0x00FF));
				densitySetFlt = dm91_eeprom::loadFloat(dm91_eeprom::ADDR_DENSITY_FLOAT, 6.176);
			}
			
			regs[i] = convertFloat(densitySetFlt, 1);
			
			break;
			
			
			case 134:

			if (writeReg)
			{
				liquefaction = static_cast<uint8_t>(Value_);
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_LIQUEFACTION, liquefaction);
			}

			regs[i] = liquefaction;
			
			break;
			
			case 135:

			if (writeReg)
			{
				overdensity = static_cast<uint8_t>(Value_);
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_OVERDENSITY, overdensity);
			}

			regs[i] = overdensity;
			
			break;
			
			
			case 136:

			if (writeReg)
			{
				warningLevel = static_cast<uint8_t>(Value_);
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_WARNING_LEVEL, warningLevel);
			}

			regs[i] = warningLevel;
			
			break;
			
			case 137:

			if (writeReg)
			{
				alarmLevel = static_cast<uint8_t>(Value_);
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_ALARM_LEVEL, alarmLevel);
			}

			regs[i] = alarmLevel;
			
			break;
			
			
			case 138:
			
			if (writeReg && pinCode)
			{
				offsetTemp = Value_ * 0.1;
				dm91_eeprom::saveSignedInt(dm91_eeprom::ADDR_OFFSET_TEMP, Value_);
			}
			
			regs[i] = offsetTemp * 10;
			
			break;
			
			case 139:
			
			if (writeReg && pinCode)
			{
				offsetHumidity_b = Value_ * 0.01;
				dm91_eeprom::saveSignedInt(dm91_eeprom::ADDR_OFFSET_HUMIDITY_B, Value_);
			}
			
			regs[i] = offsetHumidity_b * 100;
			
			break;
			
			
			
			case 140:
			
			if (writeReg && pinCode)
			{
				offsetHumidity_K = Value_ * 0.001;
				dm91_eeprom::saveSignedInt(dm91_eeprom::ADDR_OFFSET_HUMIDITY_K, Value_);
			}
			
			regs[i] = offsetHumidity_K * 1000;
			
			break;
			
			
			
			case 141:
			
			if (writeReg && pinCode)
			{
				offsetHumidity_K = float(Value_*0.01f - offsetHumidity_b) / RH_RH;
				
				dm91_eeprom::saveSignedInt(dm91_eeprom::ADDR_OFFSET_HUMIDITY_K, offsetHumidity_K * 1000);
			}
			
			regs[i] = offsetHumidity_K * 1000;
			
			break;
			
			
			
			case 142:
			
			if (writeReg && pinCode)
			{
				offsetHumidity_b = 0.01 - offsetHumidity_K * RH_RH;
				dm91_eeprom::saveSignedInt(dm91_eeprom::ADDR_OFFSET_HUMIDITY_B, offsetHumidity_b*100);
			}
			
			regs[i] = offsetHumidity_b * 100;
			
			break;
			
			
			case 143:
			
			if (writeReg && pinCode)
			{
				offsetHumidity_b = 0.0;
				dm91_eeprom::saveSignedInt(dm91_eeprom::ADDR_OFFSET_HUMIDITY_B, offsetHumidity_b*100);
				offsetHumidity_K = 1.0;
				dm91_eeprom::saveSignedInt(dm91_eeprom::ADDR_OFFSET_HUMIDITY_K, offsetHumidity_K * 1000);
				regs[i] = 1;
			}
			
			regs[i] = 0;
			
			break;
			

			
			case 148:
			
			if (writeReg && pinCode)
			{
				tian_Pmin = Value_;
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_TIAN_PMIN, tian_Pmin);
				tianTerm1 = (1.0 * (tian_Pmax+tian_Pmin))/(1.0*tian_K);
			}
			
			regs[i] = tian_Pmin;
			
			break;
			
			case 149:
			
			if (writeReg && pinCode)
			{
				tian_Pmax = Value_;
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_TIAN_PMAX, tian_Pmax);
				tianTerm1 = (1.0 * (tian_Pmax+tian_Pmin))/(1.0*tian_K);
			}

			regs[i] = tian_Pmax;
			
			break;
			
			
			case 150:
			
			if (writeReg && pinCode)
			{
				tian_K = Value_;
				dm91_eeprom::saveInt(dm91_eeprom::ADDR_TIAN_K, tian_K);
				tianTerm1 = (1.0 * (tian_Pmax+tian_Pmin))/(1.0*tian_K);
			}

			regs[i] = tian_K;
			
			break;
			
			
			case 151:
			
			if (writeReg && pinCode)
			{
				tian_Offset = Value_;
				dm91_eeprom::saveInt(dm91_eeprom::ADDR_TIAN_OFFSET, tian_Offset);
			}
			regs[i] = tian_Offset;
			
			break;
			
			
			
			
			case 152:

			if (writeReg && pinCode)
			{
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_TEMP_SETPOINT + 2U, (Value_ >> 8));
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_TEMP_SETPOINT + 3U, (Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(tempSetPoint, 0);

			break;
			
			
			
			case 153:
			
			if (writeReg && pinCode)
			{
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_TEMP_SETPOINT, (Value_ >> 8));
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_TEMP_SETPOINT + 1U, (Value_ & 0x00FF));
				
				tempSetPoint = dm91_eeprom::loadFloat(dm91_eeprom::ADDR_TEMP_SETPOINT, -20.0);
			}
			
			regs[i] = convertFloat(tempSetPoint, 1);
			
			break;
			
			
			
			case 154:
			
			regs[i] = convertFloat(DS18B20_Temp, 0);

			break;
			
			
			
			case 155:
			
			regs[i] = convertFloat(DS18B20_Temp, 1);
			
			break;
			
			
			case 156:
			
			regs[i] = PWM_SP;
			
			break;
			
			case 157:
			
			regs[i] = Heat_CPU_Current;
			
			break;
			
			case 158:
			
			if (writeReg && pinCode)
			{
				enableCPUheat = (Value_ != 0) ? 1U : 0U;
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_ENABLE_CPU_HEAT, enableCPUheat);
				if (enableCPUheat == 0U)
				{
					heaterOff();
				}
			}
			regs[i] = enableCPUheat;
			
			break;

			case 160:

			if (writeReg && pinCode)
			{
				arkticOption = (Value_ != 0) ? DM91_ARKTIC_ENABLED : DM91_ARKTIC_DISABLED;
				dm91_eeprom::writeByte(dm91_eeprom::ADDR_ARKTIC_OPTION, arkticOption);
				if (arkticOption == DM91_ARKTIC_DISABLED)
				{
					heaterOff();
				}
			}
			regs[i] = arkticOption;

			break;
			

			case 161:
			
			if (writeReg && pinCode)
			{
				dm91_eeprom::saveInt(dm91_eeprom::ADDR_INCOMING_TIMEOUT, Value_);
				incomingTimeOut = Value_;

			}
			regs[i] = incomingTimeOut;
			
			break;
			
			
			
			case 180 ... 185:
			
			regs[i] = BufSHT[registerAddr_ - 180];

			break;
			
			case 186:
			
			regs[i] = _rawHumidity;

			break;
			
			case 187:
			
			regs[i] = _rawTemperature;

			break;
			
			case 188:
			
			regs[i] = RAW_pressureTian;

			break;
			
			case 189:
			
			regs[i] = RAW_temperatureTian;

			break;
			
			

			
			
			case 190 ... 193:
			
			regs[i] = BufWT[registerAddr_ - 190];

			break;
			
			
			
			
			case 194 ... 195:
			
			regs[i] = convertFloat(pressureTian,registerAddr_ - 194);
			

			break;
			
			case 196 ... 197:
			
			regs[i] = convertFloat(temperatureTian,registerAddr_ - 196);

			break;
			

			
			
			
			case 200:
			
			regs[i] = errorInt;

			break;
			
			case 201:
			
			if (writeReg)
			if (Value_ == 1)
			errorInt = 0;

			break;
			
			
			case 202:
			
			if (writeReg)
			if (Value_ == 1)
			{
				reboot = 1;
				apply = 1;
			}
			
			regs[i] = 0;
			
			break;
			
			case 399:

			regs[i] = vipReplyQnt;

			break;
			
			
			case 400 ... 438:
			
			regs[i] = vipRespond[registerAddr_ - 400];

			break;
			
			
			
			
			case 450 ... 472:
			
			regs[i] = vipMB_ByteReply[registerAddr_ - 450];

			break;
			
			
			case 530:

			regs[i] = convertFloat(SHT_Temp_FL, 0);

			break;
			
			case 531:

			regs[i] = convertFloat(SHT_Temp_FL, 1);

			break;
			
			
			case 532:

			regs[i] = convertFloat(SHT_RH_FL, 0);

			break;
			
			case 533:

			regs[i] = convertFloat(SHT_RH_FL, 1);

			break;
			
			
			case 594:
			
			if (writeReg)
			if (Value_ == 1)
			reboot = 1;

			regs[i] = 0;

			break;
			
			case 600:
			
			if (writeReg)
			if (Value_ == 1)
			bridgeMode = 1;

			regs[i] = bridgeMode*2;
			
			break;
			
			
			case 777:

			regs[i] = watchdogQnt;

			break;
			
			
			case 800:

			if (writeReg)
			{
				if (Value_ == 2210)
				pinCode = 1;
				else pinCode = 0;
			}
			
			
			regs[i] = pinCode;
			
			break;
			
			
			
			
			case 900:
			{
				cli();
				asm volatile("jmp 0x7E00");
				
			}
			break;
			
			


			default:
			regs[i] = -1;
			break;
		}
		registerAddr_++;
	}
}




int convertFloat(float flt_, uint8_t oreder_)
{
	union {
		float flt;
		int32_t i32;
		uint8_t bytes[4];
	} u;
	
	u.flt = flt_;
	
	return (u.bytes[1 + 2 * oreder_] << 8) | u.bytes[2 * oreder_];
}


/**
 * @brief Загружает эксплуатационные настройки из EEPROM.
 *
 * @details
 * Загружаются параметры связи, серийный номер, исполнение прибора, параметры
 * газа, пороги, калибровки влажности, заводские коэффициенты датчика давления
 * и тайм-аут контроля связи.
 */
void settings()
{

	
	slaveAddr = dm91_eeprom::loadByte(dm91_eeprom::ADDR_SLAVE_ADDRESS, 1);
	

	baudRate = dm91_eeprom::loadByte(dm91_eeprom::ADDR_BAUD_RATE, 3);
	

	parity = dm91_eeprom::loadByte(dm91_eeprom::ADDR_PARITY, 0);
	

	



	densitySet = dm91_eeprom::loadInt(dm91_eeprom::ADDR_DENSITY_SET, 617);

	gasPartner = dm91_eeprom::loadByte(dm91_eeprom::ADDR_GAS_PARTNER, 0);
	serialLow = dm91_eeprom::loadInt(dm91_eeprom::ADDR_SERIAL_LOW, 0xAAAA);
	serialHigh = dm91_eeprom::loadInt(dm91_eeprom::ADDR_SERIAL_HIGH, 0xAAAA);
	
	
	modelDN = dm91_eeprom::loadByte(dm91_eeprom::ADDR_MODEL, dm910);
	getTempFromSHT = dm91_eeprom::loadByte(dm91_eeprom::ADDR_TEMP_SOURCE, 0);
	
	
	
	
	
	densitySetFlt = dm91_eeprom::loadFloat(dm91_eeprom::ADDR_DENSITY_FLOAT, 6.176);
	
	
	liquefaction = dm91_eeprom::loadByte(dm91_eeprom::ADDR_LIQUEFACTION, 133);
	overdensity = dm91_eeprom::loadByte(dm91_eeprom::ADDR_OVERDENSITY, 80);
	warningLevel = dm91_eeprom::loadByte(dm91_eeprom::ADDR_WARNING_LEVEL, 60);
	alarmLevel = dm91_eeprom::loadByte(dm91_eeprom::ADDR_ALARM_LEVEL, 50);
	offsetTemp = 0.1 * dm91_eeprom::loadSignedInt(dm91_eeprom::ADDR_OFFSET_TEMP);
	offsetHumidity_b =  0.01 * dm91_eeprom::loadSignedInt(dm91_eeprom::ADDR_OFFSET_HUMIDITY_B);
	offsetHumidity_K = 0.001 * dm91_eeprom::loadSignedInt(dm91_eeprom::ADDR_OFFSET_HUMIDITY_K);
	
	if (offsetHumidity_K == 0)
	offsetHumidity_K = 1;
	
	
	tian_Pmin = dm91_eeprom::loadByte(dm91_eeprom::ADDR_TIAN_PMIN, 1);
	tian_Pmax = dm91_eeprom::loadByte(dm91_eeprom::ADDR_TIAN_PMAX, 20);
	
	tian_K = dm91_eeprom::loadInt(dm91_eeprom::ADDR_TIAN_K, 13110);
	tian_Offset = dm91_eeprom::loadInt(dm91_eeprom::ADDR_TIAN_OFFSET, 1638);
	
	
	tianTerm1 = (1.0 * (tian_Pmax+tian_Pmin))/(1.0*tian_K);
	tianTerm1 = 0.001602;
	
	
	tempSetPoint = dm91_eeprom::loadFloat(dm91_eeprom::ADDR_TEMP_SETPOINT, -20.0);
	
	enableCPUheat = dm91_eeprom::loadByte(dm91_eeprom::ADDR_ENABLE_CPU_HEAT, 0);
	arkticOption = dm91_eeprom::loadByte(dm91_eeprom::ADDR_ARKTIC_OPTION, DM91_ARKTIC_DISABLED);
	if (arkticOption != DM91_ARKTIC_ENABLED)
	{
		arkticOption = DM91_ARKTIC_DISABLED;
	}
	
	
	incomingTimeOut = dm91_eeprom::loadInt(dm91_eeprom::ADDR_INCOMING_TIMEOUT, 5000);
	
}




unsigned long Nowdt_[2] = {0};
unsigned int lastBytesReceived_[2];
/** @brief Задержка межкадрового интервала для контроля завершения пакета. */
const uint8_t interframe_delay_ = 20;

/**
 * @brief Проверяет завершение приёма пакета по отсутствию новых байтов.
 *
 * @param ch_ Номер контролируемого канала.
 * @param length_ Текущее количество принятых байтов.
 *
 * @return 1, если пакет считается завершённым; иначе 0.
 */
uint8_t checkIncoming(uint8_t ch_, unsigned int length_)
{


	if (length_ == 0)
	{
		lastBytesReceived_[ch_] = 0;
		return 0;
	}


	if (lastBytesReceived_[ch_] != length_)
	{
		lastBytesReceived_[ch_] = length_;
		Nowdt_[ch_] = currentTime;
		return 0;
	}

	if (currentTime - Nowdt_[ch_] < interframe_delay_)
	return 0;

	lastBytesReceived_[ch_] = 0;
	return 1;

}


/**
 * @brief Рассчитывает влажностные параметры DM-91-H.
 *
 * @details
 * На основе температуры, давления и относительной влажности рассчитываются
 * точка росы, точка замерзания, пересчёт к атмосферному давлению, ppmv и ppmw.
 */
void getDewPoint()
{
	const float koef_A = 6.116441;
	const float koef_m = 7.591386;
	const float koef_Tn = 240.7263;
	const float Mw = 18.01528;
	const float Msf = 146.06;
	float Pwatm;
	float determinant_;

	if (pressure <= 0.001f)
	{
		RH_Tuned = -100.0f;
		dewPoint = -100.0f;
		frostPoint = -100.0f;
		RH_RH_Atm = -100.0f;
		dewPointAtm = -100.0f;
		frostPointAtm = -100.0f;
		PPMv = -100.0f;
		PPMm = -100.0f;
		return;
	}

	RH_Tuned = offsetHumidity_K * RH_RH + offsetHumidity_b;

	if (RH_Tuned<0.01)
	{
		RH_Tuned = 0.01;
		negativeRHflag = 1;
	}
	else
	negativeRHflag = 0;


	Pws = koef_A * pow(10, (koef_m* temperatureC / (temperatureC + koef_Tn)));
	Pw = Pws * RH_Tuned * 0.01;
	dewPoint = koef_Tn / ( (koef_m / (log10(Pw/koef_A))) - 1 );

	if (dewPoint < (-65.0))
	dewPoint = (-65.0);


	determinant_ = 1.286081 - 0.004152 * (0.009109 - dewPoint);
	frostPoint = (-1.134055 + sqrt(determinant_)) / 0.002076;
	if (frostPoint>0)
	frostPoint = 0;


	RH_RH_Atm = RH_Tuned * 100.0f/pressure;
	Pwatm = Pws * RH_RH_Atm * 0.01;
	dewPointAtm = koef_Tn / ( (koef_m / (log10(Pwatm/koef_A))) - 1 );

	determinant_ = 1.286081 - 0.004152 * (0.009109 - dewPointAtm);
	frostPointAtm = (-1.134055 + sqrt(determinant_)) / 0.002076;

	if (frostPointAtm>0)
	frostPointAtm = 0;


	const float ppmDenominator = pressure * 10.0f - Pw;
	if (ppmDenominator > 0.001f)
	{
		PPMv = 1000.0f * 1000.0f * Pw / ppmDenominator;
		PPMm = PPMv * Mw / Msf;
	}
	else
	{
		PPMv = -100.0f;
		PPMm = -100.0f;
	}

}

