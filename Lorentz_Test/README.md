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
- binary `EXIT` transition back to the text diagnostic console on the same COM port;
- firmware identity verification through the `version` command;
- validation of firmware name, version `1.02.0`, release stage and ATSAM3X8E target;
- atomic JSON report files in `reports/`, including raw diagnostic output.

Ports S1-S4, HMI, X2X and Ethernet are not required for the current USB identity test. They will be added as independent test stages with explicit PASS, FAIL or SKIPPED status.
