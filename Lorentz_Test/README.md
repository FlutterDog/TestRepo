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

- `PASS`: the complete tested path passed;
- `FIXTURE_ERROR`: the host fixture, cable, mapping, route or endpoint ownership prevents DUT evaluation;
- `FAIL`: the fixture path was established but the DUT response or internal result was incorrect;
- `SKIPPED`: the interface is disabled, not configured, unsupported or lacks exact confirmation.

## Reports

Atomic JSON reports are written to `reports/` with suffixes:

- `DIAGNOSTICS`;
- `ACTIVE_RS485`;
- `ACTIVE_ETHERNET`;
- `ACTIVE_SERVICES`;
- `HMI_ECHO`;
- `FLASH_AB`;
- `WATCHDOG_RESET`;
- `RTC_RETENTION_PREPARE` and `RTC_RETENTION_VERIFY`.

Flash A/B also creates a separate `FLASH_RECOVERY.json` before the first write.
