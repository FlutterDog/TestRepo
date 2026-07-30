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

## Implemented release 0.6.0

- persistent station configuration in `runtime/station.json`;
- Windows COM-port enumeration with VID/PID metadata;
- LCP binary USB framing and CRC32 validation;
- robust HELLO retries for Windows `usbser.sys`;
- binary `EXIT` transition back to the text diagnostic console;
- firmware identity verification through the `version` command;
- passive diagnostics for RTOS, Flash A/B, UART, SC16IS, X2X master, Ethernet, SD, battery, RTC and watchdog;
- structured parser that preserves repeated keys from separate diagnostic sections;
- station endpoint preflight with `AVAILABLE`, `BUSY`, `NOT_FOUND`, `UNREACHABLE`, `UNSUPPORTED` and `SKIPPED` states;
- host-side Modbus RTU slave engine with CRC16, FC03 and FC06 support;
- four independent active FieldSensor fixture slaves for S1-S4;
- active `LCT1114_2` X2X emulator for one configured module;
- deterministic S1-S4 register patterns and post-test verification through the LCP `field` report;
- X2X main block emulation for registers `0..93` plus waveform flag register `850`;
- separate `FIXTURE_ERROR` status when a COM port cannot be acquired;
- atomic JSON reports in `reports/`, including raw before/after diagnostics and observed RTU requests.

## Active RS-485 fixture

Close Modbus Poll and any serial terminals before starting the active test. Python must be the only owner of the configured fixture COM ports.

S1-S4 use four independent host slaves. The utility reads each port's current baud/parity from the LCP diagnostic report, starts slave address `1`, answers FC03 register `0`, count `2`, and verifies that LCP reports the unique values for that port.

The current X2X active profile supports one configured `LCT1114_2` module. The host emulator answers the chunked FC03 reads for registers `0..93` and returns zero from register `850`, so waveform transfer remains inactive. Other module types are reported as `SKIPPED` until their emulator profile is implemented.

A passive diagnostic timeout is not a DUT failure because no host slave is active. An active test can produce:

- `PASS`: Python received requests, sent responses, and LCP confirmed the expected data;
- `FIXTURE_ERROR`: Python could not acquire the configured COM port;
- `FAIL`: the fixture port was acquired, but the complete LCP-to-slave communication path did not pass;
- `SKIPPED`: no endpoint is configured or the module profile is not yet supported.
