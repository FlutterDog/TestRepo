# Lorentz Test 0.8.0 RC

Modular LCP2116 test suite for LCP Basic Firmware 1.02.0.

## Persistent backend full-test orchestration

The production path now uses an asynchronous test session:

- `POST /api/tests/lcp/full/start` creates a unique `run_id` and returns immediately;
- the backend owns USB identity, passive diagnostics, active RS-485/X2X, Ethernet, safe internal services and HMI in a deterministic sequence;
- `GET /api/tests/lcp/full/current` restores the last session after a page refresh;
- `GET /api/tests/lcp/full/{run_id}` provides live progress for polling;
- the current lifecycle and every stage transition are written atomically to `runtime/full_test_run.json`;
- closing or refreshing the browser does not stop the hardware verification;
- a backend restart during a running session converts that session to `ERROR` instead of leaving a false permanent `RUNNING` state;
- every module report is saved immediately after its stage;
- a final `FULL_TEST` JSON aggregates stage results, report paths, versions, `run_id`, the complete station snapshot and a 14-point hardware matrix;
- aggregate statuses are `PASS`, `FAIL`, `FIXTURE_ERROR` and `INCOMPLETE`;
- a critical USB/firmware failure blocks dependent stages and marks them `SKIPPED` instead of producing cascading false failures.

The pre-session synchronous `POST /api/tests/lcp/full` route remains available only for compatibility with cached frontend builds.

The aggregate filename is:

```text
LCP2116_<serial>_<timestamp>_FULL_TEST_<status>.json
```

## Exclusive hardware ownership

A process-wide backend mutex protects the DUT and fixture resources:

- a second full run is rejected with HTTP `409`;
- separate USB, diagnostics, RS-485, Ethernet, services and HMI requests cannot overlap a full run;
- Flash A/B, watchdog reset and RTC retention use the same lock;
- station settings cannot be changed while a hardware operation is running;
- the lock is released after success, report failure, runner exception or progress-state persistence failure.

This protection is enforced by the backend and therefore also applies to another browser tab.

## Operator workflow

- prominent one-click **Full LCP2116 test** button;
- automatically saves current station settings;
- creates one persistent backend session;
- shows the real current stage as `WAITING -> RUNNING -> result`;
- restores progress and the final result after `Ctrl+F5`;
- updates the six lower diagnostic cards from the same run without repeating hardware tests;
- displays the hardware matrix and aggregate JSON path;
- retains separate module buttons for targeted diagnostics and reruns;
- Flash A/B, watchdog reset and RTC retention remain separate because they write Flash, reset the DUT or require manual power removal.

Frontend build for this controller is `0.8.0-p6`.

## Safe modules

- strict active ETH1/ETH2 Modbus TCP FC03 test;
- optional PC source-IP binding;
- safe `GET_CONFIG -> VALIDATE_CONFIG -> GET_CONFIG` test without Flash write;
- microSD `SDTEST.TXT` write/read check;
- RTC synchronization and tick check;
- short RTOS progression/reserve check;
- HMI exact echo test;
- four independent FieldSensor Modbus RTU slaves;
- LCT1114_2 X2X emulator.

## Confirmed modules

- Flash A/B alternate-slot commit, reboot, exact read-back and automatic original-bundle restore;
- recovery JSON written before the first Flash mutation;
- watchdog hardware reset, CDC reconnect and firmware recovery verification;
- two-phase RTC battery-retention workflow with a complete manual power removal.

## Confirmation strings

- `FLASH A/B`
- `WATCHDOG RESET`

RTC retention uses explicit PREPARE and VERIFY buttons and sends internal phase confirmations automatically.

## First hardware feedback patches

The first LCP2116 executions confirmed USB identity, passive diagnostics, active X2X, configuration validation, microSD write/read, RTC synchronization and RTOS progression. The feedback patches:

- accept firmware RTC terminal state `done` with result `ok`;
- validate Flash and watchdog confirmation text in the browser before sending the request;
- add a 30-second VERIFY countdown after RTC PREPARE;
- return `SKIPPED` instead of DUT `FAIL` when RTC VERIFY is started before 30 seconds;
- keep the RTC baseline file after a premature VERIFY;
- separate fixture cross-wiring from DUT failure;
- mirror full-run results into the individual frontend cards;
- prevent pytest from treating the imported `TestStatus` enum as a test class.

## Validation status

The initial Windows run completed with `50 passed`. The RTC pending-state patch added one test, aggregate full-test orchestration added four tests and the persistent run controller adds four tests for concurrent exclusion, progress persistence/refresh recovery, interrupted-backend recovery and exception-safe lock release.

Expected next Windows result:

```text
59 passed, 1 warning
```

The remaining warning is the external Starlette/httpx deprecation warning. Hardware execution remains required for correctly mapped FieldSensor ports, Ethernet, HMI, Flash A/B, watchdog reset and RTC retention after a real power cycle.
