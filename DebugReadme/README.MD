### Правило хранения данных (строгий регламент)

#### Библиотека компонента (SchLib / PcbLib)
| Тип компонента | Default Designator | Default Comment | Description | User Params (Mfr/MPN/…) | Footprint model |
|---|---:|---:|---:|---:|---:|
| Пассивки (R/C/L/простые диоды) | ✅ | ❌ | ❌ | ❌ | ✅ |
| Уникальные ИС/MCU | ✅ | ❌ | ❌ | ❌ | ✅ |
| Разъёмы/клеммы | ✅ | ❌ | ❌ | ❌ | ✅ |
| Питание/модули/реле | ✅ | ❌ | ❌ | ❌ | ✅ |

#### Данные в проекте (SchDoc)
| Тип компонента | Comment | Description | Manufacturer | Manufacturer Part Number | BOM Description |
|---|---:|---:|---:|---:|---:|
| Пассивки (R/C/L/простые диоды) | ✅ | ❌ | ✅ | ✅ | ✅ |
| Уникальные ИС/MCU | ❌ | ✅ | ✅ | ✅ | ✅ |
| Разъёмы/клеммы | ❌ | ✅ | ✅ | ✅ | ✅ |
| Питание/модули/реле | ❌ | ✅ | ✅ | ✅ | ✅ |



## Примеры заполнения (эталоны)

### Пример 1 — Резистор (пассивка)
#### Библиотека компонента (SchLib / PcbLib)
| Поле | Значение |
|---|---|
| Default Designator | `R?` |
| Default Comment | *(пусто)* |
| Description | *(пусто)* |
| User Params (Mfr/MPN/…) | *(пусто)* |
| Footprint model | `RES_0603` *(пример)* |

#### Данные в проекте (SchDoc)
| Поле | Значение |
|---|---|
| Designator | `R24` *(пример)* |
| Comment | `240R` *(пример)* |
| Description | *(пусто)* |
| Manufacturer | `Yageo` *(пример)* |
| Manufacturer Part Number | `RC0603FR-07240RL` *(пример)* |
| BOM Description | `Resistor 240R 1% 0.1W 0603` *(пример)* |

---

### Пример 2 — МК SAM3X8E (уникальная ИС)
#### Библиотека компонента (SchLib / PcbLib)
| Поле | Значение |
|---|---|
| Default Designator | `U?` |
| Default Comment | *(пусто)* |
| Description | *(пусто)* |
| User Params (Mfr/MPN/…) | *(пусто)* |
| Footprint model | `LQFP144_20x20_0.5mm` *(пример)* |

#### Данные в проекте (SchDoc)
| Поле | Значение |
|---|---|
| Designator | `U1` *(пример)* |
| Comment | *(пусто)* |
| Description | `MCU SAM3X8E, ARM Cortex-M3, LQFP144` *(пример)* |
| Manufacturer | `Microchip` *(пример)* |
| Manufacturer Part Number | `ATSAM3X8EA-AU` *(пример)* |
| BOM Description | `MCU ATSAM3X8E, ARM Cortex-M3, LQFP144` *(пример)* |
