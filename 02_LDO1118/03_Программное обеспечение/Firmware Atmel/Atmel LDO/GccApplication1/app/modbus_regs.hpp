/**
 * @file modbus_regs.hpp
 * @brief Интерфейс обработчика Holding-регистров Modbus и документация адресного пространства.
 *
 * @details
 * Реализация карты регистров находится в modbus_regs.cpp. Документация вынесена в заголовок,
 * чтобы быть стабильной для Doxygen и не зависеть от комментариев внутри switch/case.
 *
 * @note
 * Адреса в описании соответствуют Holding Registers (16-bit). Фактический доступ (R/W)
 * определяется командой Modbus (FC03/FC06/FC16) и внутренними ограничениями прошивки.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Флаг запроса программной перезагрузки.
 *
 * @details
 * Устанавливается в обработчиках команд/регистров, где требуется применить параметры
 * только после рестарта (например, изменение скорости UART или типа потенциометра).
 */
extern volatile uint8_t reboot;

/**
 * @brief Обработка диапазона Holding-регистров Modbus.
 *
 * @details
 * Функция вызывается стеком Modbus как callback/делегат. В зависимости от текущей функции
 * Modbus (например, FC03/FC06/FC16) выполняет чтение или запись значений.
 *
 * В текущей архитектуре значение для записи передаётся через параметр Value_ (для FC06/FC16)
 * и интерпретируется обработчиком в контексте текущего регистра.
 *
 * @param regs       Буфер значений. При чтении заполняется для ответа; при записи может
 *                   возвращать текущее/0 в зависимости от адреса.
 * @param start_addr Адрес первого регистра в запросе.
 * @param reg_count  Количество регистров в запросе.
 * @param Value_     Значение для записи (FC06/FC16); для чтения игнорируется.
 */
void modbusProceed(int16_t* regs, int16_t start_addr, int16_t reg_count, int16_t Value_);

/**
 * @brief Завершение операции записи (подтверждение для стека Modbus).
 *
 * @details
 * Вызывается после успешного применения команды/записи, чтобы стек сформировал корректный
 * ответ на запрос записи.
 */
void replyDone(void);

/**
 * @page modbus_register_map Карта регистров Modbus
 *
 * @tableofcontents
 *
 * @section modbus_reg_general Общие правила
 * - Адреса указаны как Holding Registers (16-bit), десятичные значения.
 * - Чтение: обычно FC03 (Holding Registers Read).
 * - Запись: FC06 (Single Register) / FC16 (Multiple Registers). В прошивке факт записи определяется как writeCmd.
 * - Для командных регистров запись трактуется как действие (WRITE ONLY), возвращаемое значение в ответе обычно 0.
 * - Часть операций ограничена сервисным режимом (см. регистр 580).
 * - Паспортные регистры 9000..9025 связаны с EEPROM (поля паспортного блока) и читаются/пишутся напрямую.
 *
 * @section modbus_reg_legend Условные обозначения
 * - Доступ:
 *   - R   — чтение.
 *   - W   — запись (команда/параметр).
 *   - R/W — чтение и запись.
 *   - W*  — запись разрешена только при дополнительном условии (см. “Примечание”).
 * - Формат:
 *   - u8/u16/s16 — смысловой формат значения.
 *   - “s16 (бит-в-бит)” — перенос знакового значения через хранение как u16 без изменения битового образа.
 *
 * @section modbus_reg_work Рабочие и калибровочные регистры (0..312)
 *
 * <table class="doxtable">
 * <tr>
 *   <th>Адрес</th>
 *   <th>Доступ</th>
 *   <th>Формат</th>
 *   <th>Назначение</th>
 *   <th>Примечание / побочные эффекты</th>
 * </tr>
 *
 * <tr><td>0</td><td>R</td><td>s16</td><td>Текущая концентрация (онлайн)</td><td>Источник: outputSignals[concentration]</td></tr>
 * <tr><td>1</td><td>R</td><td>u16</td><td>Код состояния/ошибки (онлайн)</td><td>Источник: outputSignals[currentState]</td></tr>
 *
 * <tr><td>3</td><td>W</td><td>команда</td><td>Калибровка нуля: захват опорного уровня</td>
 *     <td>Запись → sensorAnalogX[0]=OUTSIGNAL[delta]+EE_ZERO_SHIFT; CheckADLadder(); gasToEEPROM(1); replyDone(); Ответ: 0</td></tr>
 *
 * <tr><td>4..9</td><td>W</td><td>команда + Value_=Y</td><td>Захват точек калибровки 1..6</td>
 *     <td>Запись → gasConcentrationY[idx]=Value_; sensorAnalogX[idx]=OUTSIGNAL[delta]; CheckADLadder(); gasToEEPROM(idx) и gasToEEPROM(idx+1); replyDone(); Ответ: 0</td></tr>
 *
 * <tr><td>10</td><td>W</td><td>команда + Value_=Y</td><td>Захват точки калибровки 7</td>
 *     <td>Запись → gasConcentrationY[7]=Value_; sensorAnalogX[7]=OUTSIGNAL[delta]; CheckADLadder(); gasToEEPROM(7); replyDone(); Ответ: 0</td></tr>
 *
 * <tr><td>11</td><td>R/W</td><td>u8</td><td>Modbus Slave Address</td>
 *     <td>Валидация 1..247; RAM: slaveAddress + IGAS_mb_X.slave; EEPROM: EE_SLAVE_ADDR; применяется сразу</td></tr>
 *
 * <tr><td>12</td><td>R/W</td><td>u16</td><td>Порог «низкий сигнал сенсора»</td>
 *     <td>RAM: lowSignal; EEPROM: EE_LOW_SIGNAL</td></tr>
 *
 * <tr><td>13</td><td>W</td><td>команда</td><td>Автокалибровка нуля</td><td>Запись → calibrZero(); replyDone(); Ответ: 0</td></tr>
 * <tr><td>14</td><td>W</td><td>команда + Value_=конц.</td><td>Калибровка span по заданной концентрации</td><td>Запись → calibrSpan(Value_); replyDone(); Ответ: 0</td></tr>
 *
 * <tr><td>15</td><td>W</td><td>команда</td><td>Расчёт и сохранение spanZero</td>
 *     <td>sz=200-SUBSTRACTED[delta]; spanZero хранится как u16 (бит-в-бит); EEPROM: EE_SPAN_ZERO_S16; replyDone(); Ответ: 0</td></tr>
 *
 * <tr><td>16</td><td>R</td><td>s16 (бит-в-бит)</td><td>spanZero (текущее значение)</td><td>Источник: spanZero (u16 с интерпретацией как s16)</td></tr>
 * <tr><td>17</td><td>R</td><td>u16</td><td>Порог «низкий сигнал сенсора» (текущее)</td><td>Источник: lowSignal</td></tr>
 *
 * <tr><td>20..27</td><td>R</td><td>u16</td><td>Калибровочные X-точки (ADC)</td><td>Источник: sensorAnalogX[0..7]</td></tr>
 * <tr><td>30..37</td><td>R</td><td>u16</td><td>Калибровочные Y-точки (концентрация)</td><td>Источник: gasConcentrationY[0..7]</td></tr>
 *
 * <tr><td>39</td><td>R</td><td>u16</td><td>Серийный номер</td><td>Источник: EEPROM EE_SERIAL_NO</td></tr>
 * <tr><td>40</td><td>R</td><td>u16</td><td>Версия прошивки</td><td>Источник: firmwareDef</td></tr>
 *
 * <tr><td>48</td><td>W</td><td>команда</td><td>Сброс к настройкам по умолчанию</td>
 *     <td>Запись → defaultSettings(); IGAS_mb_X.slave=slaveAddress; диагностика “EEPROM пустая” по EE_LOW_SIGNAL==0xFFFF; replyDone(); Ответ: 0</td></tr>
 *
 * <tr><td>49</td><td>R</td><td>u16</td><td>Маска блокировок/ошибок</td><td>Источник: EEPROM EE_BLOCK_ERROR</td></tr>
 * <tr><td>50</td><td>R/W</td><td>u16</td><td>Маска блокировок/ошибок</td><td>EEPROM: EE_BLOCK_ERROR; запись → replyDone()</td></tr>
 *
 * <tr><td>70..77</td><td>R/W</td><td>u16</td><td>Прямая запись калибровочной кривой Y</td><td>RAM: gasConcentrationY[0..7]; без EEPROM; запись → replyDone()</td></tr>
 *
 * <tr><td>78</td><td>R/W</td><td>u16</td><td>Уставка «чистого окна» (cleanSetPoint)</td><td>RAM: cleanSetPoint; EEPROM: EE_CLEAN_SETPOINT</td></tr>
 * <tr><td>79</td><td>R</td><td>u16</td><td>Уставка «чистого окна» (текущее)</td><td>Источник: cleanSetPoint</td></tr>
 *
 * <tr><td>80..87</td><td>R/W</td><td>u16</td><td>Прямая запись калибровочной кривой X (ADC)</td><td>RAM: sensorAnalogX[0..7]; без EEPROM; запись → replyDone()</td></tr>
 *
 * <tr><td>88</td><td>R/W</td><td>u8</td><td>zeroShift</td><td>EEPROM: EE_ZERO_SHIFT; запись → replyDone()</td></tr>
 * <tr><td>89</td><td>R</td><td>u8</td><td>zeroShift (текущее)</td><td>Источник: EEPROM EE_ZERO_SHIFT</td></tr>
 *
 * <tr><td>90</td><td>W</td><td>команда</td><td>Сохранение всех точек калибровки в EEPROM</td><td>Запись → gasToEEPROM(1..7); replyDone(); Ответ: 0</td></tr>
 * <tr><td>101</td><td>W</td><td>команда</td><td>Пропуск шага в автокалибровке</td><td>Запись → skipStep=1; replyDone(); Ответ: 0</td></tr>
 *
 * <tr><td>102</td><td>R/W</td><td>u8</td><td>Индекс скорости RS-485</td>
 *     <td>Валидация: Value_&lt;6 иначе 4; EEPROM: EE_BITRATE; применение через reboot=1; replyDone()</td></tr>
 *
 * <tr><td>205</td><td>R</td><td>u16</td><td>Год следующей поверки/обслуживания</td><td>Источник: EEPROM EE_NEXTVER_YEAR</td></tr>
 * <tr><td>206</td><td>R</td><td>u16</td><td>Месяц последнего обслуживания</td><td>Источник: EEPROM EE_SVC_MONTH</td></tr>
 *
 * <tr><td>207</td><td>R/W</td><td>bool(0/1)</td><td>Нагреватель: команда включения/выключения</td>
 *     <td>Запись → heaterBreaker(0/1); replyDone(); Ответ: heaterSwitch (как хранится в RAM)</td></tr>
 *
 * <tr><td>209</td><td>R</td><td>u16</td><td>Период цикла (period), мс</td><td>Источник: period</td></tr>
 * <tr><td>210</td><td>R/W</td><td>u16</td><td>Период цикла (period), мс</td><td>RAM: period; EEPROM: EE_PERIOD_MS; запись → replyDone()</td></tr>
 *
 * <tr><td>211</td><td>R</td><td>u16</td><td>Время выборки (sampleTime), мс</td><td>Источник: sampleTime</td></tr>
 * <tr><td>212</td><td>R/W</td><td>u16</td><td>Время выборки (sampleTime), мс (без EEPROM)</td><td>RAM: sampleTime; запись → replyDone()</td></tr>
 *
 * <tr><td>213</td><td>R</td><td>u16</td><td>Время свечения ИК-источника (IRTime), мс</td><td>Источник: IRTime</td></tr>
 * <tr><td>214</td><td>R/W</td><td>u16</td><td>Время свечения ИК-источника (IRTime), мс</td><td>RAM: IRTime; EEPROM: EE_IRTIME_MS; запись → replyDone()</td></tr>
 *
 * <tr><td>215</td><td>R</td><td>u8</td><td>Опция магнитной калибровки (magnetOption)</td><td>Источник: magnetOption</td></tr>
 * <tr><td>216</td><td>R/W</td><td>u8</td><td>Опция магнитной калибровки (magnetOption)</td><td>EEPROM: EE_MAGNET_OPT; запись → replyDone()</td></tr>
 *
 * <tr><td>220</td><td>R</td><td>u16</td><td>Порог стабилизации при старте (stablePirSP)</td><td>Источник: stablePirSP</td></tr>
 * <tr><td>221</td><td>R</td><td>u16</td><td>Порог шума REF (refNoiseSP)</td><td>Источник: refNoiseSP</td></tr>
 * <tr><td>222</td><td>R</td><td>u16</td><td>Порог шума GAS (gasNoiseSP)</td><td>Источник: gasNoiseSP</td></tr>
 * <tr><td>223</td><td>R</td><td>u16</td><td>Порог термоградиента (thermoGradientSP)</td><td>Источник: thermoGradientSP</td></tr>
 *
 * <tr><td>225</td><td>R/W</td><td>u16</td><td>stablePirSP (сохранение u8)</td><td>EEPROM: EE_STABLE_PIR_SP_B (u8); clamp 0..255; запись → replyDone()</td></tr>
 * <tr><td>226</td><td>R/W</td><td>u16</td><td>refNoiseSP</td><td>EEPROM: EE_REF_NOISE; запись → replyDone()</td></tr>
 * <tr><td>227</td><td>R/W</td><td>u16</td><td>gasNoiseSP</td><td>EEPROM: EE_GAS_NOISE; запись → replyDone()</td></tr>
 * <tr><td>228</td><td>R/W</td><td>u16</td><td>thermoGradientSP (сохранение u8)</td><td>EEPROM: EE_THERMO_GRAD_B (u8); clamp 0..255; запись → replyDone()</td></tr>
 *
 * <tr><td>297</td><td>R/W</td><td>bool(0/1)</td><td>Нагреватель (heaterSwitch) + сохранение</td>
 *     <td>EEPROM: EE_HEATER_SWITCH; при изменении → heaterBreaker(); replyDone()</td></tr>
 * <tr><td>298</td><td>R</td><td>bool</td><td>Нагреватель: состояние (heaterSwitch)</td><td>Источник: heaterSwitch</td></tr>
 *
 * <tr><td>299</td><td>R/W</td><td>bool(0/1)</td><td>Gain x1/x4 (смена опоры и пересчёт порогов)</td>
 *     <td>Смена диапазона → пересчёт lower/upper/gradient (×5 или ÷5), analogReference(DEFAULT/INTERNAL); EEPROM: EE_GAIN, EE_THERMO_GRAD_B, EE_UPPER_HEAT, EE_LOWER_HEAT; replyDone()</td></tr>
 *
 * <tr><td>302</td><td>R/W</td><td>u8</td><td>stableGate</td><td>EEPROM: EE_STABLE_GATE; запись → replyDone()</td></tr>
 * <tr><td>312</td><td>R</td><td>u16</td><td>Статус калибровки нагревателя (heaterTuneStatus)</td><td>Источник: heaterTuneStatus</td></tr>
 *
 * </table>
 *
 * @section modbus_reg_diag Диагностические регистры (500..900)
 *
 * <table class="doxtable">
 * <tr>
 *   <th>Адрес</th>
 *   <th>Доступ</th>
 *   <th>Формат</th>
 *   <th>Назначение</th>
 *   <th>Примечание / источник</th>
 * </tr>
 *
 * <tr><td>500..501</td><td>R</td><td>s16</td><td>Дубли регистров 0..1</td><td>outputSignals[0..1]</td></tr>
 * <tr><td>502</td><td>R</td><td>s16</td><td>Температура платы приёмника</td><td>SUBSTRACTED[tempPyro]</td></tr>
 * <tr><td>503</td><td>R</td><td>s16</td><td>Температура нагревателя</td><td>SUBSTRACTED[tempHeater]</td></tr>
 * <tr><td>504..505</td><td>R</td><td>s16</td><td>GAP-фильтрованные каналы</td><td>GAPFILTERED[0..1]</td></tr>
 * <tr><td>506</td><td>R</td><td>s16</td><td>Текущая оценка delta</td><td>OUTSIGNAL[delta]</td></tr>
 * <tr><td>507</td><td>R</td><td>u16</td><td>Биты воздействий/состояний</td><td>impactByte</td></tr>
 * <tr><td>508..509</td><td>R</td><td>s16</td><td>RAW (прямой сигнал)</td><td>RAW[0..1]</td></tr>
 * <tr><td>510..511</td><td>R</td><td>s16</td><td>Усреднённые сигналы</td><td>AVERAGED[0..1]</td></tr>
 * <tr><td>512..513</td><td>R</td><td>s16</td><td>Нормализованные сигналы</td><td>NORMALIZED[0..1]</td></tr>
 * <tr><td>514..515</td><td>R</td><td>s16</td><td>После PID-подавления шума</td><td>PIDFILTER[0..1]</td></tr>
 *
 * <tr><td>519</td><td>R</td><td>u8</td><td>Флаг детектирования газа</td><td>gasChannelFlag</td></tr>
 * <tr><td>520</td><td>R</td><td>u16</td><td>Счётчик воздействий в “тишине”</td><td>stabIdleCnt</td></tr>
 * <tr><td>521</td><td>R</td><td>u16</td><td>Счётчик стабильности после воздействия</td><td>stabActiveCnt</td></tr>
 * <tr><td>522..523</td><td>R</td><td>s16</td><td>“Угловые точки” тренда</td><td>cornerSTONE[0..1]</td></tr>
 * <tr><td>524</td><td>R</td><td>s16</td><td>Текущее spanZero</td><td>spanZero</td></tr>
 * <tr><td>525</td><td>R</td><td>s16</td><td>Разность нормализованных каналов</td><td>SUBSTRACTED[delta]</td></tr>
 * <tr><td>526</td><td>R</td><td>s16</td><td>Дублированная delta</td><td>OUTSIGNAL[delta]</td></tr>
 * <tr><td>527</td><td>R</td><td>u16</td><td>Дублированные биты воздействий</td><td>impactByte</td></tr>
 * <tr><td>528</td><td>R</td><td>u16</td><td>Модуль градиента температуры</td><td>abs(DERIVATIVE[tempPyro])</td></tr>
 *
 * <tr><td>551..566</td><td>R</td><td>s16</td><td>Фрагмент буфера TREND</td><td>TREND[ref2][0..15]</td></tr>
 *
 * <tr><td>580</td><td>W</td><td>команда</td><td>Ключ сервисного режима</td>
 *     <td>Запись значения 20118 → serviceMode=1; replyDone(); ответ: serviceMode</td></tr>
 *
 * <tr><td>590</td><td>R/W*</td><td>u16</td><td>Сервис: серийный номер</td><td>Запись только при serviceMode==1; EEPROM: EE_SERIAL_NO</td></tr>
 * <tr><td>592</td><td>R/W*</td><td>u8</td><td>Сервис: год</td><td>Запись только при serviceMode==1; EEPROM: EE_YEAR</td></tr>
 * <tr><td>593</td><td>R/W*</td><td>u8</td><td>Сервис: месяц</td><td>Запись только при serviceMode==1; clamp 1..12; EEPROM: EE_MONTH</td></tr>
 *
 * <tr><td>594</td><td>W</td><td>команда</td><td>Программная перезагрузка</td><td>Запись → reboot=1; replyDone(); Ответ: 0</td></tr>
 *
 * <tr><td>595</td><td>R/W</td><td>u16</td><td>Нижний порог гистерезиса нагревателя</td><td>EEPROM: EE_LOWER_HEAT</td></tr>
 * <tr><td>596</td><td>R/W</td><td>u16</td><td>Верхний порог гистерезиса нагревателя</td><td>EEPROM: EE_UPPER_HEAT</td></tr>
 * <tr><td>597</td><td>R</td><td>u16</td><td>Нижний порог гистерезиса (текущее)</td><td>lowerHeatLimit</td></tr>
 * <tr><td>598</td><td>R</td><td>u16</td><td>Верхний порог гистерезиса (текущее)</td><td>upperHeatLimit</td></tr>
 *
 * <tr><td>601..607</td><td>R</td><td>s16</td><td>Коэффициенты b[]</td><td>b[1..7] (индексация как в коде: addr-600)</td></tr>
 * <tr><td>611..617</td><td>R</td><td>s16</td><td>Коэффициенты k[]</td><td>k[1..7] (индексация как в коде: addr-610)</td></tr>
 *
 * <tr><td>696</td><td>W</td><td>команда</td><td>Сброс признака ADC_Alarm</td><td>Запись → ADC_Alarm=0; replyDone(); Ответ: ADC_Alarm</td></tr>
 * <tr><td>697</td><td>R</td><td>u16</td><td>Счётчик watchdog</td><td>watchdogCNT</td></tr>
 *
 * <tr><td>698</td><td>R/W</td><td>u8</td><td>Тип потенциометра (potType)</td><td>EEPROM: EE_POT_TYPE; применение через reboot=1</td></tr>
 * <tr><td>699</td><td>R/W</td><td>u8</td><td>Реверс AI (reverseAI)</td><td>EEPROM: EE_REVERSE_AI</td></tr>
 * <tr><td>700</td><td>R/W</td><td>u16</td><td>Целевое положение потенциометра (potTarget)</td><td>EEPROM: EE_POT_TARGET_B; особый формат для hc75Pot; может выставлять runPot=1</td></tr>
 *
 * <tr><td>800..803</td><td>R</td><td>s16</td><td>Максимумы/минимумы REF/GAS</td><td>fullRAW[0..3]</td></tr>
 *
 * <tr><td>900</td><td>W</td><td>команда</td><td>Переход в бутлоадер</td><td>cli(); GoToBootloader(); возврат не предполагается; Ответ: 0</td></tr>
 *
 * </table>
 *
 * @section modbus_reg_passport Паспортные регистры (9000..9025)
 * Паспортные поля 9000..9025 читаются/пишутся напрямую в EEPROM. Запись выполняется только при writeCmd.
 *
 * <table class="doxtable">
 * <tr>
 *   <th>Адрес</th>
 *   <th>Доступ</th>
 *   <th>Формат</th>
 *   <th>Поле EEPROM</th>
 *   <th>Назначение / примечание</th>
 * </tr>
 *
 * <tr><td>9000</td><td>R/W</td><td>u16</td><td>EE_GAS_CODE</td><td>Код газа</td></tr>
 * <tr><td>9001</td><td>R/W</td><td>u16</td><td>EE_UNITS_CODE</td><td>Код единиц измерения</td></tr>
 * <tr><td>9002</td><td>R/W</td><td>s16 (бит-в-бит)</td><td>EE_RANGE_MIN</td><td>Нижняя граница диапазона</td></tr>
 * <tr><td>9003</td><td>R/W</td><td>s16 (бит-в-бит)</td><td>EE_RANGE_MAX</td><td>Верхняя граница диапазона</td></tr>
 * <tr><td>9004</td><td>R/W</td><td>s16 (бит-в-бит)</td><td>EE_TMIN_C</td><td>Tmin, °C</td></tr>
 * <tr><td>9005</td><td>R/W</td><td>s16 (бит-в-бит)</td><td>EE_TMAX_C</td><td>Tmax, °C</td></tr>
 * <tr><td>9006</td><td>R/W</td><td>u16</td><td>EE_VNOM_01V</td><td>Vnom в шагах 0.1V</td></tr>
 * <tr><td>9007</td><td>R/W</td><td>u16</td><td>EE_WARRANTY_MON</td><td>Гарантийный срок, мес.</td></tr>
 * <tr><td>9008</td><td>R/W</td><td>u16</td><td>EE_HW_REV</td><td>Ревизия аппаратуры</td></tr>
 * <tr><td>9009</td><td>R/W</td><td>u16</td><td>EE_RUNTIME_LO</td><td>Наработка (low word)</td></tr>
 * <tr><td>9010</td><td>R/W</td><td>u16</td><td>EE_RUNTIME_HI</td><td>Наработка (high word)</td></tr>
 * <tr><td>9011</td><td>R/W</td><td>u16</td><td>EE_POWER_CYCLES</td><td>Число включений питания</td></tr>
 * <tr><td>9012</td><td>R/W</td><td>u16</td><td>EE_SVC_YEAR</td><td>Год обслуживания</td></tr>
 * <tr><td>9013</td><td>R/W</td><td>u16</td><td>EE_SVC_MONTH</td><td>Месяц обслуживания (clamp 1..12)</td></tr>
 * <tr><td>9014</td><td>R/W</td><td>s16 (бит-в-бит)</td><td>EE_ALARM1</td><td>Порог/уставка Alarm1</td></tr>
 * <tr><td>9015</td><td>R/W</td><td>s16 (бит-в-бит)</td><td>EE_ALARM2</td><td>Порог/уставка Alarm2</td></tr>
 * <tr><td>9016</td><td>R/W</td><td>u16</td><td>EE_OPTION_FLAGS</td><td>Флаги опций</td></tr>
 * <tr><td>9017</td><td>R/W</td><td>u16</td><td>EE_VARIANT_CODE</td><td>Код варианта исполнения</td></tr>
 * <tr><td>9018</td><td>R/W</td><td>u16</td><td>EE_LAST_ERROR</td><td>Последняя ошибка</td></tr>
 * <tr><td>9019</td><td>R/W</td><td>u16</td><td>EE_ERROR_COUNT</td><td>Счётчик ошибок</td></tr>
 * <tr><td>9020</td><td>R/W</td><td>u16</td><td>EE_NEXTVER_YEAR</td><td>Год следующей поверки</td></tr>
 * <tr><td>9021</td><td>R/W</td><td>u16</td><td>EE_NEXTVER_MONTH</td><td>Месяц следующей поверки (clamp 1..12)</td></tr>
 * <tr><td>9022</td><td>R/W</td><td>u16</td><td>EE_SENSOR_TYPE</td><td>Тип сенсора</td></tr>
 * <tr><td>9023</td><td>R/W</td><td>u16</td><td>EE_COUNTRY_CODE</td><td>Код страны</td></tr>
 * <tr><td>9024</td><td>R/W</td><td>u16</td><td>EE_INFO_CRC</td><td>CRC информационного блока</td></tr>
 * <tr><td>9025</td><td>R/W</td><td>u16</td><td>EE_RESERVED_69</td><td>Резерв</td></tr>
 *
 * </table>
 */
