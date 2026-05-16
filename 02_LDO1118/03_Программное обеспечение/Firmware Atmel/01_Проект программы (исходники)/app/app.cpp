/**
 * @file app.cpp
 * @brief Прикладной модуль прошивки: инициализация, главный цикл и основные процедуры обработки/калибровки.
 *
 * @details
 * В данном модуле сосредоточены:
 * - точка инициализации приложения (setup) и главный цикл (loop);
 * - планировщик периодических задач (10 мс / 1 с / измерительный период);
 * - обработка измерений (RAW → AVERAGED → NORMALIZED → PIDFILTER → GAPFILTERED → OUTSIGNAL);
 * - вычисление концентрации по кусочно-линейной аппроксимации (sensorAnalogX / k / b);
 * - логика детектирования газа и валидации измерения (термошок, ref-канал, устойчивость);
 * - процедуры калибровки ZERO/SPAN и настройка параметров, хранимых в EEPROM.
 *
 * Код предназначен для передачи вместе с проектом и должен позволять дальнейшую разработку как
 * на уровне прикладной логики, так и на уровне аппаратно-зависимых участков.
 */

#include "project.hpp"      // Зонтичный заголовок проекта (настройки MCU, пины, типы и т.д.)
#include "app.hpp"
#include "eeprom_map.h"
#include "modbus_regs.hpp"
#include "variables.hpp"

/**
 * @brief Указатель на активную функцию обслуживания коммуникации.
 *
 * @details
 * В зависимости от конфигурации прошивки/режима работы может указывать на различные
 * обработчики (например, режим slave). Применяется в периодических задачах (loop_10ms).
 */
void (*commFunc)();

/**
 * @brief Адрес входа в бутлоадер Optiboot (переход из приложения).
 *
 * @details
 * Значение задаётся в словах (адрес/2 относительно байтового адресного пространства).
 * Адрес извлекается из листинга бутлоадера после сборки (например, `.lst`).
 *
 * @warning
 * Адрес жёстко задан: при изменении размера бутлоадера или настроек линковки
 * адрес перехода должен быть пересчитан и обновлён.
 */
AppPtr_t GoToBootloader = (AppPtr_t)0x3F07;

/**
 * @brief Первичная инициализация оборудования и сервисов.
 *
 * @details
 * Выполняет:
 * - конфигурацию watchdog (WDT) и базовых интерфейсов;
 * - настройку GPIO;
 * - запуск SPI;
 * - запуск таймера Timer1 (период 4 мс) с ISR clampDisplay();
 * - загрузку настроек/калибровок из EEPROM (или установку дефолтов);
 * - инициализацию нагревателя/источника ИК (в зависимости от конфигурации);
 * - запуск стартовых сообщений и коммуникации.
 *
 * @warning
 * Внутри вызываются внешние функции (initPinOut(), loadSettings(), startMessages(),
 * initCommunication() и др.). Их корректность и побочные эффекты являются частью общей архитектуры.
 */
void setup()
{
    wdt_disable();                 // Отключение WDT на старте
    wdt_enable(WDTO_2S);           // Запуск WDT с таймаутом 2 секунды

    initPinOut();                  // Конфигурация линий ввода-вывода
    SPI.begin();                   // Старт SPI

    Timer1.initialize(4000);       // Период таймера 1: 4000 мкс (4 ms)
    Timer1.attachInterrupt(clampDisplay); // ISR: обслуживание CS/индикации

    loadSettings();                // Загрузка пользовательских/калибровочных параметров

    if (potType == hc75Pot)
    {
        digitalWrite(lampDrive, LOW); // Разрешить выход 595 (включение драйва источника ИК)
        // IRSource(switchOff);
    }

    heaterBreaker(heaterSwitch);   // Инициализация цепи нагревателя

    startMessages();               // Стартовая телеметрия/сообщения

    initCommunication();           // Инициализация Modbus RTU

    impactByte =  startShock << 6; // Инициализация байта событий (битовая упаковка)

    // catchIncoming();            // Опционально: первичный приём пакетов
}

/**
 * @brief Главный цикл.
 *
 * @details
 * Выполняет:
 * - обновление времени;
 * - обработку измерений;
 * - периодические задачи (1 с и ~10 мс);
 * - обслуживание системных таймеров;
 * - сброс watchdog.
 */
void loop()
{
    getTime();               // Обновление текущего времени (мс)

    measurements();          // Обработка новых данных с датчика (если доступны)

    loop_1sec();             // Задачи 1 Гц
    loop_10ms();             // Задачи ~100 Гц
    displayProceed();        // Обновление индикации

    systemTimer();           // Системные таймеры/таймауты

    wdt_reset();             // Сброс watchdog
}

/**
 * @brief Адаптивное сглаживание выходного сигнала концентрации.
 *
 * @details
 * Сглаживание outputSignals[smooth] следует за outputSignals[concentration] с шагом,
 * зависящим от величины рассогласования. Пауза между шагами также адаптивна (smoothDelay),
 * что позволяет балансировать реакцию и шумоустойчивость.
 *
 * @pre Используются глобальные переменные: currentTime, startSmoothTimer, smoothDelay, outputSignals[].
 * @warning Используется GCC-расширение диапазонов в switch (case 1 ... 10).
 */
void smoothDisplay()
{
    int16_t difference_;
    byte math_ = 0;
    byte add_ = 1;
    byte sub_ = 0;

    int16_t input_  = outputSignals[concentration]; // Целевое значение (концентрация)
    int16_t smooth_ = outputSignals[smooth];        // Текущее сглаженное значение

    if (currentTime - startSmoothTimer < smoothDelay)
        return;

    if (input_ > smooth_)
    {
        difference_ = input_ - smooth_;
        math_ = add_;
    }
    else
    {
        difference_ = smooth_ - input_;
        math_ = sub_;
    }

    startSmoothTimer = currentTime;

    switch (difference_)
    {
        case 0:
            smoothDelay = 300;
            return;

        case 1 ... 10:
            smoothDelay = 50;
            smoothMath(math_, 1);
            break;

        case 11 ... 100:
            smoothDelay = 50;
            smoothMath(math_, 10);
            break;

        case 101 ... 5000:
            smoothDelay = 25;
            smoothMath(math_, 100);
            break;

        default:
        {
            smoothDelay = 1;
            smoothMath(math_, 100);
        }
    }
}

/**
 * @brief Применить шаг сглаживания.
 *
 * @param math_  Флаг: ненулевое значение — прибавить, 0 — вычесть.
 * @param value_ Величина шага.
 */
void smoothMath(byte math_, int16_t value_)
{
    if (math_)
        outputSignals[smooth] += value_;
    else
        outputSignals[smooth] -= value_;
}

/**
 * @brief Сохранить точку калибровки газа в EEPROM и пересчитать k/b.
 *
 * @param pointer Индекс точки (ожидается 1..7).
 *
 * @details
 * Для сегмента (pointer-1 → pointer) пересчитывает:
 * - k[p] = ΔY / ΔX (с последующим хранением kint[p] = k*100);
 * - b[p] = X_{p-1} - Y_{p-1} / k[p].
 *
 * Затем сохраняет в EEPROM:
 * - X[p] в EEPROM_Start_Adress[p];
 * - Y[p] в EEPROM_Start_Adress[p]+2;
 * - kint[p] в EEPROM_Start_Adress[p]+4;
 * - b[p] в EEPROM_Start_Adress[p]+6.
 *
 * @warning
 * Требуется контроль деления на ноль при (sensorAnalogX[p] == sensorAnalogX[p-1]).
 * Логика контроля входных данных должна обеспечиваться на уровне вызывающего кода.
 */
void gasToEEPROM(byte pointer)
{
    int16_t kint[9];

    if (pointer == 8)
        return;

    k[pointer] = 1.0 * (gasConcentrationY[pointer] - gasConcentrationY[pointer - 1]) /
                 (sensorAnalogX[pointer] - sensorAnalogX[pointer - 1]);
    kint[pointer] = k[pointer] * 100;
    b[pointer] = sensorAnalogX[pointer - 1] - gasConcentrationY[pointer - 1] / k[pointer];

    // Запись значений во внутреннюю память контроллера
    if (pointer == 1)
    {
        b[1] =  sensorAnalogX[0];
        IGAS_EEPROM.saveInt(EEPROM_Start_Adress[pointer - 1], sensorAnalogX[pointer - 1]);
    }

    IGAS_EEPROM.saveInt(EEPROM_Start_Adress[pointer],     sensorAnalogX[pointer]);
    IGAS_EEPROM.saveInt(EEPROM_Start_Adress[pointer] + 2, gasConcentrationY[pointer]);
    IGAS_EEPROM.saveInt(EEPROM_Start_Adress[pointer] + 4, kint[pointer]);
    IGAS_EEPROM.saveInt(EEPROM_Start_Adress[pointer] + 6, b[pointer]);

    // eepromRecordData = 1;  // Опционально: задержка измерений на время записи EEPROM
}

/**
 * @brief Загрузить точку калибровки газа из EEPROM и восстановить k/b.
 *
 * @param pointer Индекс точки (ожидается 1..7).
 *
 * @details
 * Читает X[p], Y[p], k*100, b[p] и заполняет массивы:
 * sensorAnalogX[], gasConcentrationY[], k[], b[].
 */
void gasFromEEPROM(byte pointer)   // Загрузка данных кривой y = kx - b
{
    if (pointer == 1) // загрузка нулевой точки (X0)
        sensorAnalogX[pointer - 1] = IGAS_EEPROM.loadInt(EEPROM_Start_Adress[pointer - 1]);

    sensorAnalogX[pointer]      = IGAS_EEPROM.loadInt(EEPROM_Start_Adress[pointer]);
    gasConcentrationY[pointer]  = IGAS_EEPROM.loadInt(EEPROM_Start_Adress[pointer] + 2);
    k[pointer]                  = (IGAS_EEPROM.loadInt(EEPROM_Start_Adress[pointer] + 4) * 0.01);
    b[pointer]                  = IGAS_EEPROM.loadInt(EEPROM_Start_Adress[pointer] + 6);

    if (pointer == 1) // загрузка нулевой точки
        b[1] = sensorAnalogX[0];
}

/**
 * @brief Пересчёт X-точек кривой при изменении нулевой точки.
 *
 * @param pointer  Индекс точки 1..7 (0 — базовая нулевая точка).
 * @param oldZero_ Старое значение X[0], от которого вёлся предыдущий расчёт.
 *
 * @details
 * Масштабирует «коридор» по X относительно новой нулевой точки так, чтобы X[7]
 * оставался фиксированным репером:
 * k = (X7 - X0_new) / (X7 - X0_old);
 * Xp_new = X7 - (X7 - Xp_old)*k.
 *
 * @warning Деление на ноль при (sensorAnalogX[7] == oldZero_). Требуется защита на уровне вызывающего кода.
 * @pre Массив sensorAnalogX[] содержит валидные монотонно возрастающие значения.
 * @post Изменяет sensorAnalogX[pointer].
 */
void ZeroReTune(byte pointer, int16_t oldZero_)
{
    float k_;
    k_ = 1.0f * (sensorAnalogX[7] - sensorAnalogX[0]) / (sensorAnalogX[7] - oldZero_);
    sensorAnalogX[pointer] = sensorAnalogX[7] - (sensorAnalogX[7] - sensorAnalogX[pointer]) * k_;
}

/**
 * @brief Пересчёт точки (X,Y) при изменении верхнего предела (SPAN) по X и/или Y.
 *
 * @param pointer        Индекс точки 0..6 (точка 7 — верхний предел/репер).
 * @param oldTopLimit_X  Предыдущее значение X[7].
 * @param oldTopLimit_Y  Предыдущее значение Y[7].
 *
 * @details
 * Масштабирует отдельно X и Y шкалы так, чтобы новая сетка оставалась аффинно привязана
 * к [0]-й и [7]-й точкам: X0 и X7, Y0 и Y7.
 *
 * @warning Деление на ноль, если oldTopLimit_X == X0 или oldTopLimit_Y == Y0.
 * @post Обновляет sensorAnalogX[pointer] и gasConcentrationY[pointer].
 */
void UpperLimitReTune(byte pointer, int16_t oldTopLimit_X, int16_t oldTopLimit_Y)
{
    float k_;

    // Пересчёт X
    k_ = 1.0f * (sensorAnalogX[7] - sensorAnalogX[0]) / (oldTopLimit_X - sensorAnalogX[0]);
    sensorAnalogX[pointer] = (sensorAnalogX[pointer] - sensorAnalogX[0]) * k_ + sensorAnalogX[0];

    // Пересчёт Y
    k_ = 1.0f * (gasConcentrationY[7] - gasConcentrationY[0]) / (oldTopLimit_Y - gasConcentrationY[0]);
    gasConcentrationY[pointer] = (gasConcentrationY[pointer] - gasConcentrationY[0]) * k_ + gasConcentrationY[0];
}

/**
 * @brief Гарантирует монотонный «лестничный» рост массива sensorAnalogX[].
 *
 * @details
 * Проходит X[0]..X[6] и, если X[i] >= X[i+1], поднимает X[i+1] до X[i] + 3.
 *
 * @note Шаг «+3» — минимальный зазор по коду АЦП, используемый как эвристика.
 * @post Массив sensorAnalogX[] становится строго возрастающим.
 */
void CheckADLadder()
{
    for (uint8_t i = 0; i <= 6; i++)
    {
        if (sensorAnalogX[i] >= sensorAnalogX[i + 1])
            sensorAnalogX[i + 1] = sensorAnalogX[i] + 3;
    }
}

/**
 * @brief Инициализация дефолтных настроек и запись их в EEPROM (где требуется).
 *
 * @details
 * Инициализирует:
 * - нормирующие коэффициенты;
 * - сетку калибровки 0..7;
 * - пороги сигналов/шумов;
 * - индекс скорости RS-485;
 * - опции;
 * - тайминги измерения;
 * - лимиты нагрева и ряд служебных параметров.
 *
 * Для точек 1..7 вызывает gasToEEPROM(i) и ожидает завершения внутренних транзакций EEPROM-драйвера.
 *
 * @warning Частые записи в EEPROM вызывают износ. Функция должна вызываться ограниченно
 * (например, при первом запуске/сбросе).
 */
void defaultSettings()
{
    const uint8_t deviceTuned  = 1;   // значение флага "настроено"
    const uint8_t eepromTunedA = EE_SETTINGS_FLAG;

    // Нормирующие коэффициенты (если пусто — инициализируем нулём)
    if (EERB(EE_NORM_PIR1_x1000) == 0xFF && EERB(EE_NORM_PIR1_x1000 + 1) == 0xFF) {
        EEW(EE_NORM_PIR1_x1000, 0);
        EEW(EE_NORM_REF_x1000,  0);
    }
    normKoefPir1 = 0.001f * (float)EER(EE_NORM_PIR1_x1000);
    normKoefRef  = 0.001f * (float)EER(EE_NORM_REF_x1000);

    // Нулевая точка по умолчанию
    gasConcentrationY[0] = 0;
    sensorAnalogX[0]     = 205;

    // Точки 1..7 калибровочной кривой (примерная сетка)
    for (uint8_t i = 1; i < 8; ++i) {
        gasConcentrationY[i] = (uint16_t)(i * 750);
        sensorAnalogX[i]     = (uint16_t)(200 + i * 143);
        gasToEEPROM(i);
    }
    while (!IGAS_EEPROM.eepromRing()) { /* ожидание завершения внутренних транзакций */ }

    // Порог «низкий сигнал»
    lowSignal = 300;
    EEW(EE_LOW_SIGNAL, lowSignal);

    // Скорость RS-485 (индекс пресета, 0..6; 4 = 9600 бод)
    bitRate = 4;
    EEWB(EE_BITRATE, bitRate);

    // Пороги шумов / термоградиент
    refNoiseSP       = 150;
    gasNoiseSP       = 50;
    thermoGradientSP = 30;
    EEW (EE_REF_NOISE,  refNoiseSP);
    EEW (EE_GAS_NOISE,  gasNoiseSP);
    EEWB(EE_THERMO_GRAD_B, (uint8_t)thermoGradientSP);

    // Опции: магнитная калибровка
    magnetOption = 1;
    EEWB(EE_MAGNET_OPT, magnetOption);

    // Усиление АЦП/опора
    analogReference(INTERNAL);
    gain = 1;
    EEWB(EE_GAIN, gain);

    // Время стабилизации PIR при старте
    stablePirSP = 50;
    EEWB(EE_STABLE_PIR_SP_B, (uint8_t)stablePirSP);

    // Адрес Modbus slave
    slaveAddress = EERB(EE_SLAVE_ADDR);
    if (slaveAddress < 1 || slaveAddress > 247) {
        slaveAddress = 1;
        EEWB(EE_SLAVE_ADDR, slaveAddress);
    }

    // Нормирующие коэффициенты (единичные)
    EEW(EE_NORM_PIR1_x1000, 1);
    EEW(EE_NORM_REF_x1000,  1);

    // Тайминги цикла
    period = 1000;  // ms
    IRTime = 110;   // ms
    EEW (EE_PERIOD_MS, period);
    EEW (EE_IRTIME_MS, IRTime);

    sampleTime = 1; // ms
    EEWB(EE_SAMPLE_TIME_B, (uint8_t)sampleTime);

    // Нагреватель / стабильность
    heaterSwitch = 1;
    EEWB(EE_HEATER_SWITCH, heaterSwitch);

    stableGate = 50;
    EEWB(EE_STABLE_GATE, stableGate);

    // Сдвиг нуля
    zeroShift = 100;
    EEWB(EE_ZERO_SHIFT, (uint8_t)zeroShift);

    // Флаг "устройство настроено"
    EEWB(eepromTunedA, deviceTuned);
    settingsFlag = EERB(eepromTunedA);

    // Блокировка ошибок, clean setpoint, span zero
    blockErrorByte = 927; // слово
    EEW(EE_BLOCK_ERROR, blockErrorByte);

    cleanSetPoint = 130;
    EEW(EE_CLEAN_SETPOINT, cleanSetPoint);

    spanZero = 0;
    EEW(EE_SPAN_ZERO_S16, (int16_t)spanZero);

    // Гистерезис нагрева
    lowerHeatLimit = 9700;
    upperHeatLimit = 9800;
    EEW(EE_LOWER_HEAT, lowerHeatLimit);
    EEW(EE_UPPER_HEAT, upperHeatLimit);

    // Градиенты/счётчики (RAM)
    for (uint8_t i = 0; i < 5; ++i) gradSmooth[i] = 30;
    gradAggr[0] = 40; gradAggr[1] = 150; gradAggr[2] = 60; gradAggr[3] = 60;
    arrgCnt[0] = 2; arrgCnt[1] = 2; arrgCnt[2] = 2; arrgCnt[3] = 2;

    // Служебные ячейки
    EEW(EE_SCRATCH_300, 180);

    // Аналоговый вход / потенциометр
    reverseAI = 0;
    EEWB(EE_REVERSE_AI, (uint8_t)reverseAI);

    potTarget = 0x01;
    EEWB(EE_POT_TARGET_B, (uint8_t)potTarget);

    potType = hc75Pot;
    EEWB(EE_POT_TYPE, (uint8_t)potType);

    if (potType == hc75Pot) {
        setHCPot(0);
    } else {
        initPot();
        setPot(potTarget);
    }
}

/**
 * @brief Проверка условий ошибок и формирование состояния.
 *
 * @details
 * Сценарии:
 * - низкий сигнал каналов;
 * - отказ ИК-источника/оптики (по косвенным признакам);
 * - необходимость чистки окна (по ref-каналу);
 * - некорректное состояние EEPROM (флаг настроек);
 * - обрыв/внутренний дефект каналов;
 * - выход температуры пиродатчика за допустимые пределы.
 *
 * @post
 * Обновляет stateByte (с применением маски blockErrorByte) и outputSignals[currentState].
 * При наличии ошибок может запускать отображение ошибки.
 */
void checkError()
{
    stateByte = 0;

    if (AVERAGED[pyro1] < lowSignal)
        detectorStatus(bitFailCh1);

    if (AVERAGED[ref2] < lowSignal)
        detectorStatus(bitFailCh2);

    if ((AVERAGED[pyro1] < lowSignal) && (AVERAGED[ref2] < lowSignal))
        detectorStatus(bitIRFault);

    if ((AVERAGED[ref2] < cleanSetPoint) && (AVERAGED[ref2] > lowSignal)) {
        detectorStatus(bitCleanWindow);
        // shockTime = currentTime + 40000;
    }

    // EEPROM — проверка флага корректной инициализации
    if (settingsFlag != 1)
        detectorStatus(EEPROMFail);

    // Обрыв/браки входов (pull-up => при обрыве идёт максимум)
    if ((PIDFILTER[pyro1] < lowSignal) || (PIDFILTER[ref2] < lowSignal))
        detectorStatus(internalBrake);

    // Температура пиродатчика — выход за пределы
    if (PIDFILTER[tempPyro] > 2500 || PIDFILTER[tempPyro] < 400)
        detectorStatus(internalBrake);

    if (msgGaugeWrmUp == 1)
        stateByte = 0;

    // Маскирование запрещённых к показу ошибок
    stateByte = stateByte & blockErrorByte;

    outputSignals[currentState] = stateByte;

    if (stateByte != 0 || msgWirenError != 0)
        displayErrorMsg();
}

/**
 * @brief Формирование текстовых сообщений/индикаций на дисплее.
 *
 * @details
 * Приоритет отображения:
 * прогрев (таймер) → механический удар → Magnet Zero → Magnet Span →
 * подтверждение ("done") → термошок → ref-канал → штатный показ концентрации.
 *
 * В отдельных состояниях концентрация для интерфейсов может принудительно обнуляться.
 */
void makeMessage()
{
    if  (msgGaugeWrmUp)
    {
        byte warmTimeCalc;
        unsigned long expiredTime_ = currentTime - msgWarmUpStartTime;
        warmTimeCalc = (WarmingUpTime - expiredTime_) * 0.001; // сек

        IGAS_disp.calculateText(warmTimeCalc);

        if (warmTimeCalc < 10)
            IGAS_disp.hundreds = digit[0];
        else
            IGAS_disp.hundreds = IGAS_disp.tens;

        IGAS_disp.tens = IGAS_disp.ones;
        IGAS_disp.ones = digit[_minus];
        IGAS_disp.thousands = digit[_minus];

        if (expiredTime_ > WarmingUpTime)
        {
            msgGaugeWrmUp = 0;
            IGAS_disp.calculateText(0);
            // mechanicalShock = idle;
        }
        return;
    }

    if (startShock)
    {
        if (blinkIt)
            IGAS_disp.displayText(_H, _E, _A, _t);
        else
            IGAS_disp.displayText(_Empty, _Empty, _Empty, _Empty);
        return;
    }

    if (msgMagnetZero > 0)
    {
        if (blinkIt)
            IGAS_disp.displayText(_n, _u, _L, _L);
        else
            IGAS_disp.displayText(_Empty, _Empty, _Empty, _Empty);
        return;
    }

    if (msgMagnetSpan > 0)
    {
        if (blinkIt)
            IGAS_disp.displayText(_S, _P, _A, _n);
        else
            IGAS_disp.displayText(_Empty, _Empty, _Empty, _Empty);
        return;
    }

    if (msgRespondOk)
    {
        IGAS_disp.displayText(_d, _o, _n, _E);

        if (currentTime - msgStartTime > 2000) // 2 сек
        {
            msgRespondOk = 0;
            serviceMode = 0;
        }
        return;
    }

    // Термошок без ошибок — служебное сообщение
    if (thermoShock == active && stateByte == 0)
    {
        outputSignals[concentration] = 0;  // Modbus = 0
        if (blinkIt)
            IGAS_disp.displayText(_t, _minus, _minus, _minus);
        else
            IGAS_disp.displayText(_Empty, _Empty, _Empty, _Empty);
        return;
    }

    // Режим «reference channel» без ошибок — служебное сообщение
    if (refChannelFlag == active && stateByte == 0)
    {
        outputSignals[concentration] = 0;  // Modbus = 0
        if (blinkIt)
            IGAS_disp.displayText(_F, _minus, _minus, _minus);
        else
            IGAS_disp.displayText(_Empty, _Empty, _Empty, _Empty);
        return;
    }

    // Нормальный режим — показать сглаженную концентрацию
    if (outputSignals[currentState] == 0)
    {
        IGAS_disp.calculateText(outputSignals[smooth]);
    }
}

/**
 * @brief Сигнал «операция выполнена»: краткий показ "done".
 *
 * @post Устанавливает msgRespondOk и фиксирует msgStartTime.
 */
void replyDone()
{
    msgRespondOk = 1;
    msgStartTime = currentTime;
}

/**
 * @brief Пошаговая обработка аналоговых данных по флагу newDataArrived.
 *
 * @details
 * Последовательность шагов:
 * 1→ getTemp → 2→ averaging → 3→ normalization+trendBuffer →
 * 4→ checkTrend → 5→ noiseFilterPID → 6→ noiseGAPFILTERED → 7→ checkError →
 * 8→ newSpan → 9→ sensorToConcentration → сброс флага в 0.
 */
void processAnalogSignals()
{
    switch (newDataArrived)
    {
        case 0:
            return;

        case 1:
            getTemp();                    // чтение температур
            break;

        case 2:
            averaging();                  // усреднение
            break;

        case 3:
            normalization();              // нормализация
            trendBuffer();                // запись в тренд-буфер
            break;

        case 4:
            checkTrend();
            break;

        case 5:
            noiseFilterPID();             // снижение шума (PID-ветка)
            break;

        case 6:
            // compensateTempDrift();
            noiseGAPFILTERED();           // дополнительный фильтр
            break;

        case 7:
            checkError();                 // формирование состояния/ошибок
            break;

        case 8:
            newSpan();                    // обновление «верха»
            break;

        case 9:
            sensorToConcentration();      // перевод сенсора в концентрацию
            newDataArrived = 0;           // завершение цикла обработки
            return;

        default:
            newDataArrived = 0;
            return;
    }

    newDataArrived++;
}

/**
 * @brief Нормализация усреднённых значений.
 *
 * @details
 * Применяет коэффициенты normKoefPir1 и normKoefRef к AVERAGED[*].
 */
void normalization()
{
    NORMALIZED[pyro1]     = normKoefPir1 * AVERAGED[pyro1];
    NORMALIZED[ref2]      = normKoefRef  * AVERAGED[ref2];
    NORMALIZED[tempPyro]  = AVERAGED[tempPyro];
}

/**
 * @brief Показ стартового сообщения заданное время.
 *
 * @param msgOnDisplayTime Время показа, мс.
 *
 * @details
 * Сравнение времени выполнено по разности, корректно при переполнении счётчика.
 *
 * @warning Блокирующая функция — во время показа иные задачи не исполняются.
 */
void startUpMsgRoutine(int msgOnDisplayTime)
{
    const uint32_t start = millis();
    const uint32_t dur   = (uint32_t)msgOnDisplayTime;
    msgStartTime = start + dur;

    while ((uint32_t)(millis() - start) < dur)  // безопасно при переполнении
    {
        wdt_reset();
        displayProceed();
    }
}

/**
 * @brief Инициализация линий ввода-вывода.
 *
 * @details
 * Настройка пинов дисплея, драйвера RS-485, индикации, нагревателя, IR-источника и SPI CS.
 *
 * @warning
 * Прямой доступ к DDRx — требуется, чтобы PIN_POT_* соответствовали нужным битам порта.
 */
void initPinOut()
{
    byte CS_transferPin_ = 8;

    pinMode(CS_transferPin_, OUTPUT);   // SS дисплея
    pinMode(lampDrive,       OUTPUT);
    pinMode(txEnPin,         OUTPUT);
    pinMode(ledPin,          OUTPUT);
    pinMode(heaterPin,       OUTPUT);
    pinMode(heartBit,        OUTPUT);
    pinMode(displayEnbl,     OUTPUT);
    pinMode(irLedDrive,      OUTPUT);

    digitalWrite(txEnPin,    LOW);       // RS-485: приём
    digitalWrite(lampDrive,  LOW);
    digitalWrite(ledPin,     LOW);
    digitalWrite(CS_transferPin_, HIGH); // CS дисплея неактивен
    digitalWrite(heartBit,   HIGH);
    heaterBreaker(switchOff);
    digitalWrite(displayEnbl, LOW);

    if (potType == hc75Pot)
        digitalWrite(irLedDrive, HIGH);
    else
        digitalWrite(irLedDrive, LOW);

    DDRB |= (1<<PIN_POT_CS);
    DDRD |= (1<<PIN_POT_UD);
    DDRD |= (1<<PIN_DATA);

    cs_high();
}

/**
 * @brief Инициализация коммуникации (Modbus RTU).
 *
 * @details
 * Настраивает скорость, межкадровую паузу, delegate-обработчик и адрес слейва.
 */
void initCommunication()
{
    getBaud(bitRate);
    IGAS_mb_X.interframe_delay = modbus_pause_ms(COMM_BPS);
    IGAS_mb_X.initMB(COMM_BPS, SERIAL_8N1, modbusSize);
    IGAS_mb_X.setDelegate(modbusProceed);
    IGAS_mb_X.slave = slaveAddress;

    commFunc = &slaveMode; // режим Slave
}

/**
 * @brief Загрузка настроек из EEPROM и установка дефолтов при их отсутствии.
 */
void loadSettings()
{
    byte deviceTuned = 1;
    byte eepromTuned = 100; // исторический адрес флага «настроено»

    settingsFlag = EEPROM.read(eepromTuned);

    if (settingsFlag == deviceTuned)
        tunedSettings();
    else
        defaultSettings();
}

/**
 * @brief Чтение пользовательских/калибровочных настроек из EEPROM.
 *
 * @details
 * Загружает: адрес Modbus, 7 точек калибровочной кривой, пороги, тайминги, опции,
 * коэффициенты нормировки и ряд эксплуатационных параметров.
 */
void tunedSettings()
{
    // Адрес слейва
    slaveAddress = EERB(EE_SLAVE_ADDR);
    if (slaveAddress == 0) slaveAddress = 1;

    // 7 точек калибровочной кривой
    for (uint16_t i = 1; i <= 7; i++)
        gasFromEEPROM(i);

    // Порог низкого сигнала
    lowSignal = EER(EE_LOW_SIGNAL);

    // Скорость (индекс 0..6)
    bitRate = EERB(EE_BITRATE);

    // Шумы/пороги
    refNoiseSP        = EER(EE_REF_NOISE);
    gasNoiseSP        = EER(EE_GAS_NOISE);
    thermoGradientSP  = EERB(EE_THERMO_GRAD_B);

    // Опции
    magnetOption = EERB(EE_MAGNET_OPT);
    stablePirSP  = EERB(EE_STABLE_PIR_SP_B);

    // Тайминги цикла
    period     = EER(EE_PERIOD_MS);
    IRTime     = EER(EE_IRTIME_MS);
    sampleTime = EERB(EE_SAMPLE_TIME_B);

    // Нагреватель
    heaterSwitch = EERB(EE_HEATER_SWITCH);
    stableGate   = EERB(EE_STABLE_GATE);

    // Блокировка ошибок
    blockErrorByte = EER(EE_BLOCK_ERROR);

    // Усиление и опора АЦП
    gain = EERB(EE_GAIN);
    if (gain == 0) analogReference(DEFAULT);
    else           analogReference(INTERNAL);

    // Нормирующие коэффициенты (x1000)
    normKoefPir1 = 0.001f * (float)EER(EE_NORM_PIR1_x1000);
    normKoefRef  = 0.001f * (float)EER(EE_NORM_REF_x1000);

    // Clean setpoint
    cleanSetPoint = EER(EE_CLEAN_SETPOINT);

    // SpanZero (signed)
    spanZero = (int16_t)EER(EE_SPAN_ZERO_S16);

    // Дата выпуска
    year  = EERB(EE_YEAR);
    month = EERB(EE_MONTH);

    // Сдвиг нуля
    zeroShift = EERB(EE_ZERO_SHIFT);

    // Гистерезисы нагрева
    lowerHeatLimit = EER(EE_LOWER_HEAT);
    upperHeatLimit = EER(EE_UPPER_HEAT);

    // Реверс аналогового входа
    reverseAI = EERB(EE_REVERSE_AI);

    // Потенциометр
    potTarget = EERB(EE_POT_TARGET_B);
    potType   = EERB(EE_POT_TYPE);

    if (potType == hc75Pot) {
        setHCPot(0);
    } else {
        initPot();
        setPot(potTarget);
    }

    // Границы градиентов (RAM)
    for (uint8_t i = 0; i < 5; i++) {
        gradAggr[i]   = 50;
        gradSmooth[i] = 20;
    }
}

/**
 * @brief Управление циклом измерений (задаёт окна, IR-источник, выборку).
 *
 * @details
 * - По таймеру измерительного периода: перенос пиков в буфер, перезапуск счётчиков,
 *   запуск шага обработки (newDataArrived = 1).
 * - По таймеру IR: выключение источника.
 * - По таймеру выборки: чтение очередной выборки и обновление экстремумов.
 */
void measurements()
{
    if (taskTimer()) // старт нового измерительного цикла
    {
        dataToBuffer();
        resetTaskTimer();

        if (watchdogCNT < 100) {
            ADC_Alarm++;
        }
        watchdogCNT = 0;

        if (potType == hc75Pot) {
            setHCPot(potTarget);
            // IRSource(switchOff);
        } else {
            IRSource(turnOn);
        }

        resetIRTimer();
        resetSampleTimer();
        newDataArrived = 1;
        return;
    }

    // Выключаем IR по истечении окна
    if (currentTime - IRlampStartTime > IRTime)
    {
        if (potType == hc75Pot) {
            setHCPot(0);
            // IRSource(turnOn);
        } else {
            IRSource(switchOff);
        }
    }

    // Периодика выборки
    if (currentTime - startSampleTime > sampleTime)
    {
        watchdogCNT++;
        getSample();
    }
}

/**
 * @brief Управление IR-источником через PORTD.
 *
 * @param action Ненулевое — включить, 0 — выключить.
 *
 * @warning Прямой доступ к PORTD. Требуется исключить конфликты с другими линиями данного порта.
 */
void IRSource(byte action)
{
    uint8_t oldSREG   = SREG;
    const uint8_t ON1 = 0b00001000; // D3
    const uint8_t ON2 = 0b00010000; // D4

    cli();
    if (action) {
        PORTD |=  ON1;
        PORTD |=  ON2;
    } else {
        PORTD &= ~ON1;
        PORTD &= ~ON2;
    }
    SREG = oldSREG;
}

/**
 * @brief Перенос рассчитанных пиковых значений в рабочие буферы и очистка экстремумов.
 */
void dataToBuffer()
{
    RAW[pyro1] = channel1MaxValue - channel1MinValue;
    RAW[ref2]  = channel2MaxValue - channel2MinValue;

    fullRAW[0] = channel1MaxValue;
    fullRAW[1] = channel1MinValue;
    fullRAW[2] = channel2MaxValue;
    fullRAW[3] = channel2MinValue;

    channel1MaxValue = 0;
    channel2MaxValue = 0;

    channel1MinValue = 20000;
    channel2MinValue = 20000;
}

/**
 * @brief Чтение очередных образцов с АЦП и обновление экстремумов.
 *
 * @note
 * Читает два аналоговых канала (A0/A1) с учётом reverseAI.
 */
void getSample()
{
    int16_t Ch1 = 0;
    int16_t Ch2 = 0;

    if (reverseAI == 1)
    {
        Ch1 = analogRead(A1);
        Ch2 = analogRead(A0);
    }
    else
    {
        Ch1 = analogRead(A0);
        Ch2 = analogRead(A1);
    }

    resetSampleTimer();

    if (channel1MaxValue < Ch1) channel1MaxValue = Ch1;
    if (channel1MinValue > Ch1) channel1MinValue = Ch1;

    if (channel2MaxValue < Ch2) channel2MaxValue = Ch2;
    if (channel2MinValue > Ch2) channel2MinValue = Ch2;
}

/**
 * @brief Таймер измерительного периода.
 *
 * @return 1 — пора запускать новый цикл; 0 — ещё нет.
 */
byte taskTimer()
{
    if (currentTime - startTaskTime > period)
    {
        startTaskTime = currentTime;
        return 1;
    }
    return 0;
}

/**
 * @brief Сброс таймера измерительного периода.
 */
void resetTaskTimer()
{
    startTaskTime = currentTime;
}

/**
 * @brief Сброс таймера выборки образцов.
 * @post startSampleTime := currentTime.
 */
void resetSampleTimer()
{
    startSampleTime = currentTime;
}

/**
 * @brief Сброс таймера окна ИК-источника.
 * @post IRlampStartTime := currentTime.
 */
void resetIRTimer()
{
    IRlampStartTime = currentTime;
}

/**
 * @brief Пересчёт измерений в концентрацию по кусочно-линейной аппроксимации.
 *
 * @details
 * По значению OUTSIGNAL[delta] выбирается интервал по sensorAnalogX[] и применяется
 * кусочно-линейная зависимость y = k[i] * (f - b[i]). Результат ограничивается диапазоном
 * [0; gasLimit].
 *
 * @warning
 * Требуется обеспечить монотонность sensorAnalogX[] (см. CheckADLadder()).
 */
void sensorToConcentration()
{
    int16_t f_ = OUTSIGNAL[delta];

    // Кламп к нижней опоре
    if (f_ <= sensorAnalogX[0])
        f_ = sensorAnalogX[0];

    // Основные интервалы [X[i-1]; X[i])
    for (byte i = 1; i <= 6; i++)
    {
        if (f_ >= sensorAnalogX[i - 1] && f_ < sensorAnalogX[i])
        {
            int32_t y = (int32_t)(k[i] * (f_ - b[i])); // расчёт в расширенном типе

            if (y < 0) y = 0;
            if (y > gasLimit) y = gasLimit;

            outputSignals[concentration] = (int16_t)y;
            return;
        }
    }

    // Хвост ≥ X6: используем k[7]/b[7]
    if (f_ >= sensorAnalogX[6])
    {
        int32_t y = (int32_t)(k[7] * (f_ - b[7]));

        if (y > gasLimit) y = gasLimit;
        if (y < 0)        y = 0;

        outputSignals[concentration] = (int16_t)y;
        return;
    }
}

/**
 * @brief Системные таймеры уровня-события: timer10 / timer2 / timerSec.
 *
 * @details
 * - timer10: период @ref timer10delay;
 * - timer2 : период @ref timer2delay;
 * - timerSec: период @ref timerSecDelay.
 *
 * Сравнение ведётся по разности времени — корректно при переполнении.
 */
void systemTimer()
{
    // timer10
    if (timer10 == 1) {
        timer10 = 0;
        starttimer10 = currentTime;
    }
    if ((currentTime - starttimer10) > timer10delay)
        timer10 = 1;

    // timer2
    if (timer2 == 1)
        timer2 = 0;

    if ((currentTime - startTimer2) > timer2delay) {
        timer2 = 1;
        startTimer2 = currentTime;
    }

    // timerSec
    if (timerSec == 1) {
        timerSec = 0;
        startTimerSec = currentTime;
    }
    if ((currentTime - startTimerSec) > timerSecDelay)
        timerSec = 1;
}

/**
 * @brief Обновление производных/состояний и вычисление «дельты» и температур.
 *
 * @details
 * - SUBSTRACTED[delta] = GAPFILTERED[ref2] - GAPFILTERED[pyro1];
 * - OUTSIGNAL[delta] = spanZero + SUBSTRACTED[delta]*shiftCompensKoef (или 200 при отсутствии газа/ошибках);
 * - нижний кламп OUTSIGNAL[delta] (190);
 * - температуры — линейные преобразования от gain.
 */
void newSpan()
{
    SUBSTRACTED[delta] = GAPFILTERED[ref2] - GAPFILTERED[pyro1];
    SUBSTRACTED[ref2]  = GAPFILTERED[ref2];

    if (gasDetected == active && stateByte == 0)
        OUTSIGNAL[delta] = spanZero + SUBSTRACTED[delta] * shiftCompensKoef;
    else
        OUTSIGNAL[delta] = 200;

    if (OUTSIGNAL[delta] < 190)
        OUTSIGNAL[delta] = 190;

    OUTSIGNAL[ref2] = GAPFILTERED[ref2];

    if (gain == 0) {
        SUBSTRACTED[tempPyro]   = 0.488f  * GAPFILTERED[tempPyro]   + 2230.0f;
        SUBSTRACTED[tempHeater] = 0.488f  * AVERAGED[tempHeater]    + 2230.0f;
    } else {
        SUBSTRACTED[tempPyro]   = 0.1075f * GAPFILTERED[tempPyro]   + 2230.0f;
        SUBSTRACTED[tempHeater] = 0.1075f * AVERAGED[tempHeater]    + 2230.0f;
    }
}

/**
 * @brief Обновить текущее время из millis().
 * @post currentTime := millis().
 */
void getTime()
{
    currentTime = millis();
}

/**
 * @brief Пролистывание и вывод кодов ошибок на индикатор.
 *
 * @details
 * Печатает Err + номер ошибки (0..8), продвигая lastError и сбрасывая changeStatusMsg.
 */
void displayErrorMsg()
{
    int16_t mask = 0;

    if (changeStatusMsg == 0)
        return;

    changeStatusMsg = 0;

    for (byte i = lastError; i < 9; i++)
    {
        mask = (int16_t) (1 << i);
        if (stateByte & mask)
        {
            IGAS_disp.displayText(_E, _r, _r, lastError);
            lastError++;
            return;
        }
        lastError++;
    }

    if (lastError > 8)
        lastError = 0;
}

/**
 * @brief Снятие «сырых» температур с АЦП.
 * @note Каналы: A2=tempPyro, A3=tempHeater.
 */
void getTemp()
{
    RAW[tempPyro]   = analogRead(A2);
    RAW[tempHeater] = analogRead(A3);
}

/**
 * @brief Окно усреднения последних 10 значений для каждого канала.
 *
 * @details
 * Формирует AVERAGED[*] как сумму 10 последних значений в кольце averageBuffer[*][10].
 * Деление на 10 не выполняется: остальные части обработки согласованы с таким масштабом.
 */
void averaging()
{
    averageBuffer[pyro1][averageCounter]      = RAW[pyro1];
    averageBuffer[ref2][averageCounter]       = RAW[ref2];
    averageBuffer[tempPyro][averageCounter]   = RAW[tempPyro];
    averageBuffer[tempHeater][averageCounter] = RAW[tempHeater];

    AVERAGED[pyro1]      = 0;
    AVERAGED[ref2]       = 0;
    AVERAGED[tempPyro]   = 0;
    AVERAGED[tempHeater] = 0;

    for (byte i = 0; i < 10; i++)
    {
        AVERAGED[pyro1]      += averageBuffer[pyro1][i];
        AVERAGED[ref2]       += averageBuffer[ref2][i];
        AVERAGED[tempPyro]   += averageBuffer[tempPyro][i];
        AVERAGED[tempHeater] += averageBuffer[tempHeater][i];
    }

    averageCounter++;
    if (averageCounter >= 10)
        averageCounter = 0;
}

/**
 * @brief Установить бит ошибки в stateByte.
 * @param bitErr_ Позиция бита (0..15).
 */
void detectorStatus(int16_t bitErr_)
{
    stateByte = stateByte | (1 << bitErr_);
}

/**
 * @brief Ежесекундные задачи: потенциометр, индикация, магнитная калибровка, программный рестарт.
 *
 * @details Вызывается при поднятом флаге timerSec.
 */
void loop_1sec()
{
    if (timerSec)
    {
        if (runPot != 0)
        {
            if (potType == hc75Pot) setHCPot(potTarget);
            else                    setPot(potTarget);
            runPot = 0;
        }

        changeStatusMsg = 1;
        blinkIt = !blinkIt;
        digitalWrite(ledPin, blinkIt);

        // Магнитная калибровка (Zero/Span)
        if (magnetOption == 1)
        {
            // Zero
            if (analogRead(A6) < 500 && msgMagnetZero == 0) msgMagnetZero = 1;

            if (analogRead(A6) > 500 && msgMagnetZero == 1) {
                msgMagnetZero = 2;
                magnetTimeOut = 2000;
                magnetTimer = currentTime;
                startMagnetDelay = 1;
            }

            if (analogRead(A6) < 500 && msgMagnetZero == 3) msgMagnetZero = 4;

            if (analogRead(A6) < 500 && msgMagnetZero == 4) {
                msgMagnetZero = 5;
                calibrZero();
            }

            if (analogRead(A6) > 500 && msgMagnetZero == 5) msgMagnetZero = 0;

            // Span
            if (analogRead(A7) < 500 && msgMagnetSpan == 0) msgMagnetSpan = 1;

            if (analogRead(A7) > 500 && msgMagnetSpan == 1) {
                msgMagnetSpan = 2;
                magnetTimeOut = 2000;
                magnetTimer = currentTime;
                startMagnetDelay = 1;
            }

            if (analogRead(A7) < 500 && msgMagnetSpan == 3) msgMagnetSpan = 4;

            if (analogRead(A7) < 500 && msgMagnetSpan == 4) {
                msgMagnetSpan = 5;
                calibrSpan(5200);
            }

            if (analogRead(A7) > 500 && msgMagnetSpan == 5) msgMagnetSpan = 0;

            // Тайминги «тишины»
            if ((currentTime - magnetTimer) > magnetTimeOut && startMagnetDelay == 1)
            {
                if (msgMagnetSpan == 3)  { msgMagnetSpan = 0; startMagnetDelay = 0; }
                if (msgMagnetZero == 3)  { msgMagnetZero = 0; startMagnetDelay = 0; }

                if (msgMagnetSpan == 2)  { magnetTimeOut = 20000; magnetTimer = currentTime; msgMagnetSpan = 3; }
                if (msgMagnetZero == 2)  { magnetTimeOut = 10000; magnetTimer = currentTime; msgMagnetZero = 3; }
            }
        }

        // Программный рестарт
        if (reboot)
        {
            wdt_disable();
            wdt_enable(WDTO_15MS);
            for (;;) { }
        }
    }
}

/**
 * @brief Периодические задачи ~каждые 10 мс: связь, сглаживание, обработка, сообщения, EEPROM-ring.
 */
void loop_10ms()
{
    if (timer10)
    {
        commFunc();             // Modbus RTU
        smoothDisplay();        // сглаживание отображения
        processAnalogSignals(); // шаговый конвейер обработки
        makeMessage();          // приоритетные сообщения
        IGAS_EEPROM.eepromRing();

        if (resetTransmitLED == 1) {
            resetTransmitLED = 0;
            IGAS_mb_X.transmitLED = 0;
        }
        if (IGAS_mb_X.transmitLED != 0)
            resetTransmitLED = 1;
    }
}

/**
 * @brief Запуск «задержки» (таймер в мс).
 */
void startDelay(uint16_t time_)
{
    wirenTimerSP = time_;
}

/**
 * @brief Проверка окончания «задержки».
 * @return 1 — время вышло, 0 — ещё нет.
 */
byte finishDelay()
{
    if ((currentTime - startDelayTime) > wirenTimerSP)
        return 1;
    return 0;
}

/**
 * @brief GAP-фильтр с порогами: при длительном превышении ±2 подтягивает GAPFILTERED к PIDFILTER.
 *
 * @details
 * STRIKE_H/L считают длительность «выброса»; при превышении strikeLimit выполняется
 * обновление GAPFILTERED[i] = 0.5*(PIDFILTER[i] + GAPFILTERED[i]).
 */
void noiseGAPFILTERED()
{
    byte const strikeLimit = 10;

    for (byte i = 0; i < (totalChannels - 1); i++)
    {
        if (PIDFILTER[i] > GAPFILTERED[i] + 2) { STRIKE_H[i]++; STRIKE_L[i] = 0; }
        else if (PIDFILTER[i] < GAPFILTERED[i] - 2) { STRIKE_H[i] = 0; STRIKE_L[i]++; }
        else { STRIKE_H[i] = 0; STRIKE_L[i] = 0; }

        if ((STRIKE_H[i] > strikeLimit) || (STRIKE_L[i] > strikeLimit))
            GAPFILTERED[i] = 0.5f * (PIDFILTER[i] + GAPFILTERED[i]);
    }
}

/**
 * @brief Адаптивный фильтр PIDFILTER: выбор «мягкого» или «агрессивного» коэффициента по шуму в TREND[10].
 *
 * @details
 * Для каждого канала вычисляет min/max в окне, расширяет границы на ±0.2% и выбирает:
 * - SMOOTHfilter при попадании текущего PIDFILTER в окно;
 * - иначе AGGRESSfilter.
 */
void noiseFilterPID()
{
    int16_t noiseMax = 0;
    int16_t noiseMin = 30000;
    float powerShift;

    for (byte i = 0; i < (totalChannels - 1); i++)
    {
        noiseMax = 0;
        noiseMin = 20000;

        for (byte j = 0; j < 10; j++) {
            if (TREND[i][j] > noiseMax) noiseMax = TREND[i][j];
            if (TREND[i][j] < noiseMin) noiseMin = TREND[i][j];
        }

        noiseMin = noiseMin * 0.998f;
        noiseMax = noiseMax * 1.002f;

        if  (PIDFILTER[i] > noiseMin && noiseMax > PIDFILTER[i])
            powerShift = SMOOTHfilter[i];
        else
            powerShift = AGGRESSfilter[i];

        PIDFILTER[i] += powerShift * (1.0f * NORMALIZED[i] - PIDFILTER[i]);
    }
}

/**
 * @brief Стартовая последовательность сообщений на индикаторе.
 *
 * @details
 * Последовательность: FULL → I → IG → IGA → IGAS → версия → адрес → скорость.
 * После завершения устанавливает флаг прогрева msgGaugeWrmUp и фиксирует msgWarmUpStartTime.
 *
 * @warning Последовательность блокирующая (использует startUpMsgRoutine()).
 */
void startMessages()
{
    byte currentMessage = 0;
    byte finished = 0;

    while (!finished)
    {
        switch (currentMessage)
        {
            case 0: IGAS_disp.displayText(_FULL, _FULL, _FULL, _FULL); break;
            case 1: IGAS_disp.displayText(_I, _Empty, _Empty, _Empty); break;
            case 2: IGAS_disp.displayText(_I, _G, _Empty, _Empty);     break;
            case 3: IGAS_disp.displayText(_I, _G, _A, _Empty);         break;
            case 4: IGAS_disp.displayText(_I, _G, _A, _S);             break;
            case 5: IGAS_disp.calculateText(firmwareDef); IGAS_disp.thousands = digit[_F]; break;
            case 6: IGAS_disp.calculateText(slaveAddress); IGAS_disp.thousands = digit[_A]; break;
            case 7: IGAS_disp.calculateText(COMM_BPS);                 break;
            default:
                finished = 1;
                msgGaugeWrmUp = 1;
                msgWarmUpStartTime = millis();
                break;
        }
        currentMessage++;
        startUpMsgRoutine(IGASonstart);
    }
}

/**
 * @brief Обслуживание отображения: если дисплей готов — отправляем буфер.
 */
void displayProceed()
{
    if (IGAS_disp.dispTrigger == 0)
        IGAS_disp.displayMessage(IGAS_mb_X.transmitLED);
}

/**
 * @brief Расчёт скорости UART по индексу предустановки.
 *
 * @param command_ 0..6: 1200, 2400, 4800, 9600, 19200, 38400; всё, что ≥6 — 9600.
 */
void getBaud(byte command_)
{
    byte multiplier = 1 << command_;

    if (command_ < 6)
        COMM_BPS = 1200 * multiplier;
    else
        COMM_BPS = 9600;
}

/**
 * @brief Включение/выключение «брейкера» нагревателя через PORTD.
 *
 * @param state_ 1 — включить, 0 — выключить.
 */
void heaterBreaker(byte state_)
{
    if (state_ == 1) {
        PORTD = PORTD | breakerOn;
        breakerStatus = 1;
    } else {
        PORTD = PORTD & breakerOff;
        breakerStatus = 0;
    }
}

/**
 * @brief Короткий кламп CS дисплея по SPI (используется в ISR таймера).
 */
void clampDisplay()
{
    IGAS_disp.CS_clampSPIPin();
}

/**
 * @brief Режим Modbus-слейва: обслуживание стека.
 */
void slaveMode()
{
    IGAS_mb_X.update();
}

/**
 * @brief Калибровка SPAN: перенос верхней точки и пересчёт сетки.
 *
 * @param Value_ Новое значение верхней точки по Y (gasConcentrationY[7]).
 *
 * @details
 * 1) Сохраняет старые X7/Y7;
 * 2) Устанавливает новую X7 из текущего OUTSIGNAL[delta], Y7 = Value_;
 * 3) Ретюнит точки 6..0 аффинно (UpperLimitReTune), затем выправляет «лестницу» X;
 * 4) Сохраняет точки 1..7 в EEPROM;
 * 5) Показывает "done".
 */
void calibrSpan(int16_t Value_)
{
    oldTopLimitX = sensorAnalogX[7];
    oldTopLimitY = gasConcentrationY[7];
    gasConcentrationY[7] = Value_;
    sensorAnalogX[7] = OUTSIGNAL[delta];

    for (int8_t j = 6; j >= 0; --j)
        UpperLimitReTune((uint8_t)j, oldTopLimitX, oldTopLimitY);

    CheckADLadder();

    for (uint8_t j = 1; j <= 7; j++)
        gasToEEPROM(j);

    replyDone();
}

/**
 * @brief Калибровка ZERO: нормировка каналов и перенос нулевой точки.
 *
 * @details
 * - Вычисляет normKoefPir1/normKoefRef для приведения каналов к 5000;
 * - Обновляет cleanSetPoint;
 * - Переносит X0 (с учётом zeroShift);
 * - Сбрасывает тренды/фильтры к базовым значениям;
 * - Пересчитывает X-точки относительно нового нуля и сохраняет точки в EEPROM;
 * - Показывает "done".
 */
void calibrZero()
{
    // Нормировка каналов к ~середине шкалы
    normKoefPir1 = 5000.0f / AVERAGED[pyro1];
    normKoefRef  = 5000.0f / AVERAGED[ref2];
    IGAS_EEPROM.saveInt(EE_NORM_PIR1_x1000, (int16_t)(normKoefPir1 * 1000.0f));
    IGAS_EEPROM.saveInt(EE_NORM_REF_x1000,  (int16_t)(normKoefRef  * 1000.0f));
    shiftCompensKoef = 1;

    // Базовый сдвиг
    spanZero = 200;

    // Порог «чистки окна»
    if (AVERAGED[pyro1] < AVERAGED[ref2])
        cleanSetPoint  = (int16_t)(0.50f * AVERAGED[pyro1]);
    else
        cleanSetPoint  = (int16_t)(0.50f * AVERAGED[ref2]);
    IGAS_EEPROM.saveInt(EE_CLEAN_SETPOINT, cleanSetPoint);

    // Перенос X0 и сброс буферов/углов
    oldZero = sensorAnalogX[0];
    sensorAnalogX[0] = 200 + zeroShift;

    for (uint8_t j = 0; j <= trendLimit; j++)
    {
        TREND[pyro1][j] = 5000;
        TREND[ref2][j]  = 5000;
    }
    GAPFILTERED[pyro1] = GAPFILTERED[ref2] = 5000;
    PIDFILTER[pyro1]   = PIDFILTER[ref2]   = 5000;
    AVERAGED[pyro1]    = AVERAGED[ref2]    = 5000;
    cornerSTONE[pyro1] = cornerSTONE[ref2] = 5000;

    OUTSIGNAL[delta] = 200;

    for (uint8_t j = 1; j <= 7; j++)
        ZeroReTune(j, oldZero);

    CheckADLadder();

    for (uint8_t j = 1; j <= 7; j++)
        gasToEEPROM(j);

    replyDone();
}

/**
 * @brief Кольцевой буфер тренда нормированных значений.
 */
void trendBuffer()
{
    tail++;
    if (tail > trendLimit)
        tail = 0;

    for (byte i = 0; i < (totalChannels - 1); i++)
        TREND[i][tail] = NORMALIZED[i];
}

/**
 * @brief Пересчёт производных/состояний по трендам и установка флагов.
 *
 * @details
 * 1) Производные по pyro1/ref2/tempPyro;
 * 2) cornerDrvtive (для ref2 в отдельных режимах);
 * 3) calcGasFlag() — машина состояний.
 */
void checkTrend()
{
    calcDrvtive(pyro1);
    calcDrvtive(ref2);
    calcDrvtive(tempPyro);

    if (refChannelFlag != idle)
        cornerDrvtive = cornerSTONE[ref2] - PIDFILTER[ref2];

    calcGasFlag();
}

/**
 * @brief Производная по кольцевому буферу TREND[*] в точке tail.
 * @param signalCh_ Индекс канала.
 */
void calcDrvtive(byte signalCh_)
{
    if (tail == trendLimit)
        DERIVATIVE[signalCh_] = TREND[signalCh_][0] - TREND[signalCh_][trendLimit];
    else
        DERIVATIVE[signalCh_] = TREND[signalCh_][tail+1] - TREND[signalCh_][tail];
}

/**
 * @brief Машина состояний детектирования газа/реф-канала/термошока; формирует impactByte.
 *
 * @details
 * Включает:
 * - стартовую стабилизацию;
 * - термошок;
 * - контроль ref-канала;
 * - контроль gas-канала;
 * - обновление базовой линии (zeroDet) и итоговые флаги.
 */
void calcGasFlag()
{
    int16_t absREF    = abs(DERIVATIVE[ref2]);
    int16_t negPIR    = DERIVATIVE[pyro1];
    int16_t absPIR    = abs(DERIVATIVE[pyro1]);
    int16_t absThermo = abs(DERIVATIVE[tempPyro]);
    byte getBase = 0;

    // Стартовая стабилизация
    if (startShock != idle)
    {
        if (absPIR < stablePirSP && absREF < stablePirSP)  stabIdleCnt++;
        else                                               stabIdleCnt = 0;

        if (stabIdleCnt > 60) {
            startShock = idle;
            getCornerPoints(pyro1, 10);
            getCornerPoints(ref2,  10);
        }
        return;
    }

    // Отрицательная производная по pyro1 в дальнейшей логике не используется
    if (DERIVATIVE[pyro1] < 0) DERIVATIVE[pyro1] = 0;
    absPIR = DERIVATIVE[pyro1];

    // Термошок
    switch (thermoShock)
    {
        case idle:
            if (absThermo > thermoGradientSP) {
                thermoShock = active;
                refChannelFlag = idle;
                gasChannelFlag = idle;
            }
            break;

        case active:
            if (absThermo < thermoGradientSP) {
                thermoCounter++;
                if (thermoCounter > 20) {
                    thermoShock = idle;
                    getCornerPoints(pyro1, 10);
                    getCornerPoints(ref2,  10);
                    getBase = 1;
                }
            } else {
                thermoCounter = 0;
            }
            break;
    }

    // Reference channel
    if (thermoShock == 0)
    {
        switch (refChannelFlag)
        {
            case idle:
                if (absREF > refNoiseSP) {
                    refActiveCnt++;
                    if (refActiveCnt > 4) {
                        refChannelFlag = active;
                        getCornerPoints(ref2, 30);
                        getBase = 1;
                        refActiveCnt = 0;
                    }
                } else {
                    stabIdleCnt++;
                }

                if (stabIdleCnt > 20) {
                    stabIdleCnt = trendLimit;
                    if (abs(cornerSTONE[ref2] - GAPFILTERED[ref2]) > 50) {
                        getCornerPoints(ref2, 10);
                    }
                }
                break;

            case active:
                stabIdleCnt = 0;
                if ((absREF < refNoiseSP * 0.5) || cornerDrvtive < refNoiseSP * 0.5)
                    stabActiveCnt++;
                else
                    stabActiveCnt = 0;

                if (stabActiveCnt > stabFlagCntSP)
                {
                    stabActiveCnt = 0;
                    refChannelFlag = idle;

                    if (abs(cornerSTONE[ref2] - GAPFILTERED[ref2]) > mechanicalDriftSP)
                        refCornerBackUp = cornerSTONE[ref2];

                    for (byte i = 0; i <= trendLimit; i++)
                        TREND[ref2][i] = PIDFILTER[ref2];
                }
                break;
        }

        // Gas channel
        switch (gasChannelFlag)
        {
            case idle:
                if (negPIR < 0 && absPIR > gasNoiseSP) {
                    gasChannelFlag = steady;
                    getCornerPoints(pyro1, 30);
                    getBase = 1;
                    negativeGasTime = currentTime;
                    negativeGas = 1;
                    break;
                }

                if ((negPIR > 0) && (absPIR > gasNoiseSP) && (absPIR > (4 * absREF))) {
                    gasActiveCnt++;
                    if (gasActiveCnt > 7) {
                        gasChannelFlag = active;
                        if (negativeGas == 0) {
                            getCornerPoints(pyro1, 30);
                            getBase = 1;
                        } else {
                            negativeGas = 0;
                        }
                    }
                } else {
                    stabIdleCnt++;
                    gasActiveCnt = 0;
                }

                if (stabIdleCnt > 250)
                    stabIdleCnt = trendLimit;
                break;

            case steady:
                if (spanZero + SUBSTRACTED[delta] > 200) {
                    gasChannelFlag = idle;
                } else {
                    if (currentTime - negativeGasTime > 300000UL) // 5 мин
                        gasChannelFlag = idle;
                }
                break;

            case active:
                if (refChannelFlag == idle && (spanZero + SUBSTRACTED[delta] < 299)) {
                    gasChannelFlag = idle;
                    stabIdleCnt = 0;
                }
                break;
        }
    }

    if (getBase) zeroDet();

    // Итоговый флаг «газ обнаружен» (логика по проекту)
    gasDetected = gasChannelFlag & (~refChannelFlag);

    // impactByte: g | t<<2 | r<<4 | s<<6
    impactByte =  gasDetected | (thermoShock << 2) | (refChannelFlag << 4) | (startShock << 6);
}

/**
 * @brief Взять «опорную» точку (cornerSTONE[channel_]) из кольца TREND со смещением.
 *
 * @param channel_    Канал.
 * @param startPoint_ Смещение от tail назад (в отсчётах кольца).
 *
 * @details Усредняет 10 точек начиная с tail - startPoint_.
 */
void getCornerPoints(byte channel_, byte startPoint_)
{
    unsigned long buffer_ = 0;
    int16_t tail_ = tail - startPoint_;
    if (tail_ < 0) tail_ += trendLimit;

    for (byte i = 0; i < 10; i++)
    {
        if (tail_ >= trendLimit) {
            buffer_ += TREND[channel_][trendLimit];
            tail_ = 0;
        } else {
            buffer_ += TREND[channel_][tail_];
            tail_++;
        }
    }

    cornerSTONE[channel_] = (int16_t)(0.1f * buffer_);
}

/**
 * @brief Обновить «нулевую линию» и коэффициент сдвига.
 *
 * @details
 * spanZero = 200 + (cornerPyro - cornerRef);
 * shiftCompensKoef = 5000 / cornerPyro.
 */
void zeroDet()
{
    spanZero = 200 + cornerSTONE[pyro1] - cornerSTONE[ref2];
    shiftCompensKoef = 5000.0f / cornerSTONE[pyro1];
}

/* === Низкоуровневые линии управления потенциометром (битовые) === */

/** @brief Установить CS потенциометра в 1. */
void cs_high(void){ PORTB |=  (1<<PIN_POT_CS); }
/** @brief Установить CS потенциометра в 0. */
void cs_low (void){ PORTB &= ~(1<<PIN_POT_CS); }
/** @brief Установить UD потенциометра в 1. */
void ud_high(void){ PORTD |=  (1<<PIN_POT_UD); }
/** @brief Установить UD потенциометра в 0. */
void ud_low (void){ PORTD &= ~(1<<PIN_POT_UD); }

/**
 * @brief Пошаговый потенциометр (0..63) — «кликаем» ножкой UD под CS.
 *
 * @param potTarget_ Желаемая позиция (кламп 0..63).
 * @return 0 — выполнено.
 */
byte setPot(uint8_t potTarget_)
{
    if (potTarget_ > 63) potTarget_ = 63;

    if (potTarget_ < currentPot)
    {
        ud_low();
        _delay_us(20);
        cs_low();
        _delay_us(20);

        for (byte i = currentPot; i > potTarget_; i--)
        {
            ud_low();
            _delay_us(20);
            ud_high();
            _delay_us(20);
            wdt_reset();
        }
    }
    else if (potTarget_ > currentPot)
    {
        ud_high();
        _delay_us(20);
        cs_low();
        _delay_us(20);

        for (byte i = currentPot; i < potTarget_; i++)
        {
            ud_low();
            _delay_us(20);
            ud_high();
            _delay_us(20);
            wdt_reset();
        }
    }

    cs_high();
    currentPot = potTarget_;
    return 0;
}

/** @brief Установить линию DATA потенциометра в 1. */
void potData_high(void){ PORTD |=  (1<<PIN_DATA); }
/** @brief Установить линию DATA потенциометра в 0. */
void potData_low (void){ PORTD &= ~(1<<PIN_DATA); }

/**
 * @brief HC-потенциометр: сдвиговый протокол 8 бит, младший бит = enable.
 *
 * @param potTarget_ 0..255; при нуле бит0 очищается, иначе — устанавливается.
 * @return 0 — выполнено.
 */
byte setHCPot(uint8_t potTarget_)
{
    if (potTarget_ > 255) potTarget_ = 255;

    if (potTarget_ == 0)  bitClear(potTarget_, 0);
    else                  bitSet  (potTarget_, 0);

    ud_high();
    _delay_us(10);
    cs_low();
    _delay_us(10);

    for (byte i = 0; i < 8; i++)
    {
        ud_low();
        if (bitRead(potTarget_, 7 - i)) potData_high(); else potData_low();
        _delay_us(10);
        ud_high();
        _delay_us(10);
        wdt_reset();
    }

    cs_high();
    currentPot = potTarget_;
    return 0;
}

/**
 * @brief Инициализация пошагового потенциометра — «загон» в ноль 65 импульсами.
 * @return 0 — выполнено.
 */
byte initPot()
{
    ud_low();
    _delay_us(20);
    cs_low();
    _delay_us(20);

    for (byte i = 0; i < 65; i++)
    {
        ud_low();
        _delay_us(20);
        ud_high();
        _delay_us(20);
        wdt_reset();
    }

    cs_high();
    currentPot = 0;
    return 0;
}

/* === Modbus RTU межкадровая пауза === */

#define MB_FAST_BAUD_THRESHOLD 19200UL /**< Порог «быстрых» скоростей */
#define MB_MIN_FAST_MS 2U              /**< Минимальная пауза для fast, мс */
#define MB_BITS_PER_CHAR 11            /**< 1старт + 8бит + parity/none + стоп */
#define MB_EXTRA_MS 1U                 /**< Доп. запас, мс */

/**
 * @brief Расчёт межкадровой паузы Modbus RTU в миллисекундах.
 *
 * @param baud Скорость UART (бод).
 * @return Пауза между фреймами, мс.
 *
 * @details
 * База по стандарту: 3.5 * Tchar, где Tchar = MB_BITS_PER_CHAR/baud.
 * Расчёт ведётся в мкс с округлением вверх до мс. Для fast-baud задаётся минимум,
 * дополнительно вводится небольшой запас для устойчивости на линии.
 */
uint16_t modbus_pause_ms(uint32_t baud)
{
    uint32_t base_us = (3500000UL * (uint32_t)MB_BITS_PER_CHAR + baud - 1) / baud; // ceil
    uint16_t base_ms = (uint16_t)((base_us + 999UL) / 1000UL);                     // ceil → ms

    if (baud > MB_FAST_BAUD_THRESHOLD && base_ms < MB_MIN_FAST_MS) {
        base_ms = MB_MIN_FAST_MS;
    }

    uint16_t pause_ms = (uint16_t)(base_ms + MB_EXTRA_MS);
    return pause_ms;
}
