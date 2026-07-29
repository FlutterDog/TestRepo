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
