# Lorentz Test 0.8.0 RC

Modular LCP2116 test suite for LCP Basic Firmware 1.02.0.

## Backend full-test orchestration

- one `POST /api/tests/lcp/full` request owns the complete safe verification;
- backend runs USB identity, passive diagnostics, active RS-485/X2X, Ethernet, safe internal services and HMI in a deterministic sequence;
- every module report is saved immediately after its stage;
- a final `FULL_TEST` JSON aggregates stage results, report paths and a 14-point hardware matrix;
- aggregate statuses are `PASS`, `FAIL`, `FIXTURE_ERROR` and `INCOMPLETE`;
- a critical USB/firmware failure blocks dependent stages and marks them `SKIPPED` instead of producing cascading false failures;
- the hardware matrix separates evaluated DUT points from fixture errors and unconfigured points.

The aggregate filename is:

```text
LCP2116_<serial>_<timestamp>_FULL_TEST_<status>.json
```

## Operator workflow

- prominent one-click **Full LCP2116 test** button;
- automatically saves current station settings;
- sends one backend full-test request;
- displays sequential stage results, the hardware matrix and the aggregate JSON path;
- retains separate module buttons below for targeted diagnostics and reruns;
- Flash A/B, watchdog reset and RTC retention remain separate because they write Flash, reset the DUT or require manual power removal.

## New safe modules

- strict active ETH1/ETH2 Modbus TCP FC03 test;
- optional PC source-IP binding;
- safe `GET_CONFIG -> VALIDATE_CONFIG -> GET_CONFIG` test without Flash write;
- microSD `SDTEST.TXT` write/read check;
- RTC synchronization and tick check;
- short RTOS progression/reserve check;
- HMI exact echo test.

## New confirmed modules

- Flash A/B alternate-slot commit, reboot, exact read-back and automatic original-bundle restore;
- recovery JSON written before the first Flash mutation;
- watchdog hardware reset, CDC reconnect and firmware recovery verification;
- two-phase RTC battery-retention workflow with a complete manual power removal.

## Confirmation strings

- `FLASH A/B`
- `WATCHDOG RESET`

RTC retention uses explicit PREPARE and VERIFY buttons and sends internal phase confirmations automatically.

## First hardware feedback patch

The first LCP2116 execution confirmed USB identity, passive diagnostics, active X2X, configuration validation, microSD write/read and RTOS progression. The patch after that run:

- accepts firmware RTC terminal state `done` with result `ok`;
- validates Flash and watchdog confirmation text in the browser before sending the request;
- adds a 30-second VERIFY countdown after RTC PREPARE;
- returns `SKIPPED` instead of DUT `FAIL` when RTC VERIFY is started before 30 seconds;
- keeps the RTC baseline file after a premature VERIFY;
- prevents pytest from treating the imported `TestStatus` enum as a test class.

## Validation status

The initial Windows run completed with `50 passed`. The RTC pending-state patch added one test and backend full-test orchestration adds four tests for deterministic order, fixture/incomplete aggregation, USB prerequisite blocking and aggregate JSON naming. Expected next Windows result is `55 passed` plus the external Starlette/httpx deprecation warning. Hardware execution remains required for connected FieldSensor ports, Ethernet, HMI, Flash A/B, watchdog reset and RTC retention after a real power cycle.
