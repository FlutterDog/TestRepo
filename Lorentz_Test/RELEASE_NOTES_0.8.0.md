# Lorentz Test 0.8.0

Released on 2026-08-04 for LCP Basic Diagnostic Firmware 1.02.0.

## Release status

Version 0.8.0 is the hardware-validated baseline of the local LCP2116 production and service test utility. The firmware is unchanged at 1.02.0.

Validated on physical controller `LCP_8_16`:

- USB binary protocol, diagnostic console and firmware identity;
- internal RTOS, SRAM, Flash, UART, Ethernet-controller, microSD, battery, RTC and watchdog diagnostics;
- active FieldSensor S1 through a host Modbus RTU slave emulator;
- active X2X through an LCT1114_2 emulator;
- active ETH1 Modbus TCP FC03 over the complete S1 -> LCP -> Ethernet data path;
- HMI exact byte echo;
- safe configuration transport, microSD write/read, RTC synchronization and RTOS progression;
- Flash A/B alternate-slot write, reboot, exact read-back and automatic restoration of the original bundle;
- watchdog hardware reset, USB reconnect and recovery reset;
- RTC retention after complete removal of USB and main power.

The final post-reset full run completed with 10 configured hardware points passed, zero DUT failures and zero fixture errors. S2, S3, S4 and ETH2 were intentionally left unconfigured because those fixture channels were not available; their test paths remain in the released utility.

## Persistent backend full-test orchestration

The production path uses an asynchronous test session:

- `POST /api/tests/lcp/full/start` creates a unique `run_id` and returns immediately;
- the backend executes USB identity, passive diagnostics, active RS-485/X2X, Ethernet, safe internal services and HMI in a deterministic sequence;
- `GET /api/tests/lcp/full/current` restores the last session after a page refresh;
- `GET /api/tests/lcp/full/{run_id}` provides live progress for polling;
- lifecycle and stage transitions are written atomically to `runtime/full_test_run.json`;
- closing or refreshing the browser does not stop the hardware verification;
- an interrupted `WAITING` or `RUNNING` session becomes `ERROR` after backend restart;
- every module report is saved immediately;
- the final `FULL_TEST` JSON contains stage reports, versions, `run_id`, station snapshot and a 14-point hardware matrix;
- aggregate statuses are `PASS`, `FAIL`, `FIXTURE_ERROR` and `INCOMPLETE`;
- critical USB/firmware failure blocks dependent stages as `SKIPPED` instead of creating cascading false failures.

The legacy synchronous `POST /api/tests/lcp/full` route remains only for cached frontend compatibility.

## Exclusive hardware ownership

A process-wide backend mutex protects DUT and fixture resources:

- concurrent full runs are rejected with HTTP `409`;
- individual tests cannot overlap a full run;
- Flash A/B, watchdog reset and RTC retention use the same lock;
- station settings cannot change during a hardware operation;
- lock release is guaranteed after success, report failure, runner exception or progress-state persistence failure.

## Operator workflow

- one-click full LCP2116 test;
- automatic station-settings save;
- persistent backend session with live stage progress;
- recovery after `Ctrl+F5`;
- individual diagnostic cards populated from the same run without repeating tests;
- aggregate hardware matrix and JSON path;
- separate engineering buttons for targeted reruns;
- separate confirmed Flash A/B and watchdog tests;
- two-phase RTC retention workflow.

Frontend build: `0.8.0-p6`.

## Confirmation strings

- `FLASH A/B`
- `WATCHDOG RESET`

## Validation evidence

Hardware evidence from the final sequence:

- Flash A/B: original slot 1 -> test slot 2 -> restored slot 1, original CRC `0x270AB171`, `restored=true`, `recovery_required=false`;
- watchdog: boot count 5 -> 6, USB reconnected on COM10, recovery performed, watchdog re-enabled;
- RTC retention: boot count 6 -> 7, approximately 3972 seconds elapsed, RTC/PC difference 2 seconds, battery comparator `ok`;
- final full run: 10/10 configured points PASS, 0 FAIL, 0 FIXTURE_ERROR, 4 intentionally SKIPPED.

The Windows test suite expected for this release is:

```text
64 passed, 1 warning
```

The remaining warning is external to the project and originates from the Starlette/httpx compatibility layer.
