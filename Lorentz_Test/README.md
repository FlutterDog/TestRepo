# Lorentz Test

Local Windows production and service test utility for Lorentz controllers and modules.

Current scope: **LCP2116 with LCP Basic Firmware v1.02.0**.

The application runs one local Python/FastAPI process. That process owns serial and network interfaces. A browser is used only as the local user interface.

## Development start

Windows PowerShell:

```powershell
cd Lorentz_Test
py -3.11 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install -e ".[dev]"
lorentz-test
```

Open `http://127.0.0.1:8765` when the browser does not open automatically.

## Scope exclusions

PNX, DM91 and density meters are not part of this project stage.

## Implemented release 0.8.0

### Persistent sequential full verification

The production workflow creates an asynchronous backend session:

```text
POST /api/tests/lcp/full/start
```

The response contains a unique `run_id`. The frontend reads live progress through:

```text
GET /api/tests/lcp/full/{run_id}
GET /api/tests/lcp/full/current
```

The backend owns the complete safe sequence:

1. USB identity and firmware;
2. passive diagnostics;
3. active S1-S4 and X2X;
4. active ETH1/ETH2;
5. configuration transport, microSD, RTC and RTOS;
6. HMI echo.

Current lifecycle and stage transitions are written atomically to:

```text
runtime/full_test_run.json
```

Closing or refreshing the browser does not stop the test. The current run and its result are restored after `Ctrl+F5`. A backend process restart during `WAITING` or `RUNNING` marks that run as `ERROR`; the interrupted result is not treated as valid.

Each completed stage writes its detailed module JSON immediately. The backend then writes one aggregate report containing:

- unique `run_id`;
- backend, frontend and firmware versions;
- complete station configuration snapshot;
- raw and effective status of every stage;
- paths to all module reports;
- a fixed 14-point hardware verification matrix;
- evaluated, pending, fixture-error and failed point counts;
- names of pending and failed hardware points.

A critical USB/firmware failure blocks dependent stages and records them as `SKIPPED` instead of producing cascading false failures.

The synchronous compatibility route remains available for cached older frontends:

```text
POST /api/tests/lcp/full
```

### Exclusive hardware ownership

All hardware-changing or hardware-accessing POST operations share one process-wide backend mutex.

This prevents:

- two full tests from running simultaneously;
- a separate test from opening the same COM port during a full run;
- Flash A/B, watchdog or RTC-retention operations from overlapping another hardware test;
- station settings from changing during a running test;
- conflicts caused by a second browser tab.

A conflicting request returns HTTP `409` and identifies the current hardware owner. The lock is released after success, runner exception, report error or run-state persistence error.

### Identity and passive diagnostics

- persistent station configuration in `runtime/station.json`;
- Windows COM-port enumeration with VID/PID metadata;
- LCP binary USB framing and CRC32 validation;
- robust HELLO retries and binary `EXIT` transition for Windows `usbser.sys`;
- firmware identity verification through `version`;
- passive semantic diagnostics for RTOS, Flash A/B, UART, SC16IS, X2X master, Ethernet, SD, battery, RTC and watchdog;
- endpoint preflight and explicit `FIXTURE_ERROR` classification.

### Active serial interfaces

- four independent host-side Modbus RTU slaves for S1-S4;
- deterministic values and read-back through the LCP `field` report;
- automatic detection of crossed fixture COM assignments;
- active `LCT1114_2` X2X emulator for registers `0..93` and flag register `850`;
- HMI echo at `9600 8N1` with three exact frames;
- local COM and `tcp://host:port` fixture endpoints.

Close Modbus Poll and serial terminals before active serial tests. Python must be the only owner of configured fixture endpoints.

### Shared HMI/X2X adapter

HMI and X2X may use the same endpoint only when:

- `shared_hmi_x2x_adapter=true`;
- both endpoint fields are populated;
- both fields refer to the same endpoint.

X2X still conflicts with USB and S1-S4 endpoints. With one physically movable HMI/X2X adapter, the full run tests X2X and records HMI as `SKIPPED/INCOMPLETE` with an instruction to move the adapter and execute HMI separately. With two separate endpoints, HMI runs automatically.

### Active Ethernet

Each enabled ETH port is tested independently:

1. read the LCP `eth` report;
2. match station DUT IP to active firmware IP;
3. require physical link up;
4. optionally bind the client to a selected PC source IP;
5. connect to TCP port 502;
6. issue Modbus TCP FC03 for holding registers `0..11`;
7. validate MBAP and PDU fields;
8. confirm increased DUT request/response counters with no new transport errors.

Missing link, route, adapter or firewall path is `FIXTURE_ERROR`. A malformed response after connection is `FAIL`.

### Safe active internal services

- binary `GET_CONFIG`;
- schema, length and CRC32 verification;
- `VALIDATE_CONFIG` of the unchanged active bundle;
- second `GET_CONFIG` and byte-for-byte comparison;
- no `PUT_CONFIG`, no Flash write and no reboot;
- `sd test`, which overwrites and reads back only `SDTEST.TXT`;
- RTC synchronization, read-back and tick check;
- RTOS tick/uptime progression and heap/stack reserve checks.

### Flash A/B write and restore

This separate confirmed test intentionally changes persistent configuration:

1. read and validate the exact original 104-byte bundle;
2. write an atomic recovery JSON before any mutation;
3. change only the temporary S4 baudrate field;
4. validate and commit to the inactive Flash slot;
5. reboot and verify the alternate slot and exact test CRC;
6. write the exact original bundle to the other slot;
7. reboot and verify byte-for-byte restoration;
8. attempt emergency automatic restore on any intermediate error.

The operator must enter `FLASH A/B`. When `recovery_required=true`, use the path in `backup_file` before further DUT use.

### Watchdog reset and USB recovery

The operator must enter `WATCHDOG RESET`. The test verifies the safe pre-state, arms `watchdog test reset`, waits for the hardware reset and firmware software-recovery reset, reconnects to the CDC device and checks boot count, recovery marker, reset type, watchdog enable and status register.

### RTC battery retention

The two-phase flow is intentionally manual:

- PREPARE synchronizes RTC, records boot count and battery comparator state;
- the operator removes USB and all main power for at least 30 seconds;
- VERIFY checks increased boot count, RTC elapsed time against PC elapsed time and the battery comparator.

A successful retention test proves operation through the tested power-off interval. It does not measure actual battery voltage; firmware exposes only the comparator state.

## Result classification

- `PASS`: every required configured hardware point was evaluated and passed;
- `INCOMPLETE`: no DUT failure was confirmed, but at least one point was disabled, unconfigured or skipped;
- `FIXTURE_ERROR`: the host fixture, cable, mapping, route, report storage or endpoint ownership prevents complete DUT evaluation;
- `FAIL`: the fixture path was established but the DUT response or internal result was incorrect;
- `SKIPPED`: an individual interface or stage is disabled, not configured, blocked, unsupported or lacks exact confirmation.

## Reports

Atomic JSON reports are written to `reports/` with suffixes:

- `FULL_TEST`;
- `DIAGNOSTICS`;
- `ACTIVE_RS485`;
- `ACTIVE_ETHERNET`;
- `ACTIVE_SERVICES`;
- `HMI_ECHO`;
- `FLASH_AB`;
- `WATCHDOG_RESET`;
- `RTC_RETENTION_PREPARE` and `RTC_RETENTION_VERIFY`.

Aggregate report format:

```text
LCP2116_<serial>_<timestamp>_FULL_TEST_<status>.json
```

Flash A/B also creates a separate `FLASH_RECOVERY.json` before the first write.
