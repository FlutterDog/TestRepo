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

## Implemented MVP slice

- persistent station configuration in `runtime/station.json`;
- Windows COM-port enumeration with VID/PID metadata;
- LCP binary USB framing and CRC32 validation;
- robust HELLO retries for Windows `usbser.sys`;
- PASS/FAIL evaluation for protocol v1, schema v1, 104-byte bundle and capabilities;
- binary `EXIT` transition back to the text diagnostic console;
- firmware identity verification through the `version` command;
- validation of firmware name, version `1.02.0`, release stage and ATSAM3X8E target;
- non-destructive diagnostics for RTOS, Flash A/B, FieldSensor, RS-485, SC16IS, X2X, Ethernet, SD, battery, RTC and watchdog;
- structured parser that preserves repeated keys from separate diagnostic sections;
- semantic PASS/FAIL/SKIPPED checks for the confirmed LCP Basic 1.02.0 output format;
- atomic JSON report files in `reports/`, including raw and structured diagnostic data.

Diagnostic status is based on report content. Configured X2X modules must be online and the RTC must agree with the PC clock within five minutes. External S1-S4 and Ethernet communication checks remain SKIPPED until their fixtures are configured; internal UART and controller checks still run.
