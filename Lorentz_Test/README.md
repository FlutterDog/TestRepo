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

## Implemented release 0.7.0

### Identity and passive diagnostics

- persistent station configuration in `runtime/station.json`;
- Windows COM-port enumeration with VID/PID metadata;
- LCP binary USB framing and CRC32 validation;
- robust HELLO retries for Windows `usbser.sys`;
- binary `EXIT` transition back to the text diagnostic console;
- firmware identity verification through the `version` command;
- passive diagnostics for RTOS, Flash A/B, UART, SC16IS, X2X master, Ethernet, SD, battery, RTC and watchdog;
- structured diagnostic parser that preserves repeated keys from separate sections;
- station endpoint preflight with `AVAILABLE`, `BUSY`, `NOT_FOUND`, `UNREACHABLE`, `UNSUPPORTED` and `SKIPPED` states.

### Active RS-485 fixture

- host-side Modbus RTU slave engine with CRC16, FC03 and FC06 support;
- four independent FieldSensor fixture slaves for S1-S4;
- active `LCT1114_2` X2X emulator for one configured module;
- deterministic S1-S4 register patterns and verification through the LCP `field` report;
- X2X main block emulation for registers `0..93` plus waveform flag register `850`;
- automatic detection of crossed fixture COM assignments;
- separate `FIXTURE_ERROR` status for busy, absent, inaccessible or incorrectly mapped fixture endpoints.

Close Modbus Poll and serial terminals before active RS-485 tests. Python must be the only owner of the configured fixture ports.

### Active Ethernet

Each enabled port is tested independently:

1. read the LCP `eth` report;
2. require the configured DUT IP to match the active firmware IP;
3. require physical link up;
4. optionally bind the client socket to the configured PC source IP;
5. connect to TCP port 502;
6. issue Modbus TCP FC03 for holding registers `0..11`;
7. validate MBAP transaction ID, protocol ID, unit ID, length, function and byte count;
8. read the LCP `eth` report again and confirm increased request/response counters with no new transport errors.

A missing link, route, source adapter or firewall path is `FIXTURE_ERROR`. A malformed response after TCP connection is `FAIL`.

### Active internal services

The safe service test performs:

- binary `GET_CONFIG`;
- schema, length and CRC32 verification;
- `VALIDATE_CONFIG` of the unchanged active bundle;
- a second `GET_CONFIG` and byte-for-byte comparison;
- no `PUT_CONFIG`, no Flash write and no reboot;
- `sd test`, which overwrites and reads back only `SDTEST.TXT`;
- RTC synchronization from the PC, read-back and a three-second tick check;
- two RTOS snapshots with tick/uptime progression and heap/stack reserve checks.

### HMI echo

The HMI test opens the configured fixture endpoint at `9600 8N1`, sends three frames of different sizes and contents, and requires exact echo from the firmware diagnostic HMI service. Local COM ports and raw serial-server endpoints in `tcp://host:port` form are supported.

## Result classification

- `PASS`: the complete tested path passed;
- `FIXTURE_ERROR`: the host fixture, cable, mapping, network route or endpoint ownership prevents DUT evaluation;
- `FAIL`: the fixture path was established but the DUT response or internal result was incorrect;
- `SKIPPED`: the interface is disabled, not configured or not supported by the current emulator profile.

## Reports

Every module writes an atomic JSON report to `reports/`:

- `DIAGNOSTICS`;
- `ACTIVE_RS485`;
- `ACTIVE_ETHERNET`;
- `ACTIVE_SERVICES`;
- `HMI_ECHO`.

Reports include raw before/after diagnostics, parsed checks, fixture counters and exact error classification.
