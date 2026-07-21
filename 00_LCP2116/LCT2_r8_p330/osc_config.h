#ifndef OSC_CONFIG_H
#define OSC_CONFIG_H

// Дискретизация и формат хранения
#define SAMPLE_PERIOD_US             100u      ///< 100 мкс между сэмплами

#define SAMPLE_BYTES_PER_PHASE         2u      ///< 1 сэмпл/фаза = 2 байта (12 бит в 16)

// Объём FRAM и секции
#define FRAM_TOTAL_SIZE              8192u     ///< 8 КБ
#define STRIKES_MAX                     3u     ///< максимум ударов в сессии


#define PHASE_SAMPLES_MAX   400u
#define latestDetectedCurrent   360u
#define FRAM_HDR_SIZE       40u


#if PHASE_SAMPLES_MAX > 65535
# error "PHASE_SAMPLES_MAX должен быть <= 65535 для 16-битных регистров"
#endif

/// Размер окна для RMS в режиме IDLE (число выборок)
#define RMS_WINDOW_N         2000u   ///< 2000 * 50 мкс = 100 мс, подстрой как тебе нужно
#define EMU_Sine_HZ            50u
// Начальный фазовый сдвиг синуса в МОМЕНТ перехода в BUSY (в градусах)
#define EMU_START_SINE_PHASE      0u     // например 0..359

// Межфазный сдвиг между A?B?C (в градусах), по умолчанию 120°
#define EMU_SINE_INTERPHASE_SHIFT       120u

// ---- Предрасчёт фиксированных шагов/сдвигов в единицах 0..65535 ----
#define DEG_TO_U16PH(deg)        ((uint16_t)((uint32_t)(deg) * 65536ull / 360ull))

// шаг фазового аккумулятора на ОДНУ выборку:  phase_step = f_sig/fs * 65536
// fs = 1e6 / SAMPLE_PERIOD_US
#define EMU_PHASE_STEP_U16   ((uint16_t)(( (uint32_t)EMU_Sine_HZ * 65536ull * (uint32_t)SAMPLE_PERIOD_US ) / 1000000ull))

/// Небольшой запас (в тиках) вокруг точки детекта дуги
#ifndef ARC_PAD_TICKS_OFF
#define ARC_PAD_TICKS_OFF   6u   ///< для отключения
#endif
#ifndef ARC_PAD_TICKS_ON
#define ARC_PAD_TICKS_ON    6u   ///< для включения
#endif

#endif