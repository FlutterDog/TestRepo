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
- station endpoint preflight with `AVAILABLE`, `BUSY`, `NOT_FOUND`, `UNREACHABLE`, `UNSUPPORTED` and `SKIPPED` states;
- atomic JSON report files in `reports/`, including endpoint access, raw and structured diagnostic data.

The passive USB diagnostic snapshot evaluates internal LCP state. It does not start external Modbus RTU slaves on S1-S4 and does not synchronize the RTC; those checks are therefore `SKIPPED` until their dedicated active test stages run. A configured X2X module is evaluated from the LCP runtime report and must be online. Endpoint acquisition problems such as a COM port held by Modbus Poll are reported separately as station errors and do not by themselves mark the LCP hardware as failed.
