"""FastAPI entry point for the local Lorentz Test utility."""

from __future__ import annotations

import threading
import webbrowser

import uvicorn
from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from lorentz_test import __version__
from lorentz_test.models.full_test import LcpFullTestResult
from lorentz_test.models.serial import SerialPortInfo
from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import (
    ConfirmedTestRequest,
    LcpActiveEthernetResult,
    LcpActiveRs485Result,
    LcpActiveServicesResult,
    LcpDiagnosticsResult,
    LcpFlashAbResult,
    LcpHelloRequest,
    LcpHelloResult,
    LcpHmiResult,
    LcpRtcRetentionResult,
    LcpWatchdogResetResult,
)
from lorentz_test.paths import frontend_dir
from lorentz_test.reporting.json_report import report_writer
from lorentz_test.serial_ports import list_serial_ports
from lorentz_test.services.lcp_active_ethernet import run_lcp_active_ethernet
from lorentz_test.services.lcp_active_rs485 import run_lcp_active_rs485
from lorentz_test.services.lcp_active_services import run_lcp_active_services
from lorentz_test.services.lcp_diagnostics import run_lcp_diagnostic_snapshot
from lorentz_test.services.lcp_flash_ab import run_lcp_flash_ab
from lorentz_test.services.lcp_full_test import run_lcp_full_test
from lorentz_test.services.lcp_hello import run_lcp_hello
from lorentz_test.services.lcp_hmi import run_lcp_hmi_echo
from lorentz_test.services.lcp_rtc_retention import (
    prepare_rtc_retention,
    verify_rtc_retention,
)
from lorentz_test.services.lcp_watchdog_reset import run_lcp_watchdog_reset
from lorentz_test.station_store import station_store

APP_HOST = "127.0.0.1"
APP_PORT = 8765

app = FastAPI(title="Lorentz Test", version=__version__)


def _load_station() -> StationConfig:
    try:
        return station_store.load()
    except RuntimeError as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc


def _save_report(save_method: object, result: object) -> None:
    try:
        save_method(result)
    except RuntimeError as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc


@app.get("/api/health")
def health() -> dict[str, str]:
    return {"status": "ok", "version": __version__}


@app.get("/api/ports", response_model=list[SerialPortInfo])
def get_ports() -> list[SerialPortInfo]:
    try:
        return list_serial_ports()
    except RuntimeError as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc


@app.post("/api/tests/lcp/full", response_model=LcpFullTestResult)
def test_lcp_full(request: LcpHelloRequest) -> LcpFullTestResult:
    station = _load_station()
    result = run_lcp_full_test(request, station)
    _save_report(report_writer.save_lcp_full_test, result)
    return result


@app.post("/api/tests/lcp/hello", response_model=LcpHelloResult)
def test_lcp_hello(request: LcpHelloRequest) -> LcpHelloResult:
    result = run_lcp_hello(request, _load_station())
    _save_report(report_writer.save_lcp_hello, result)
    return result


@app.post("/api/tests/lcp/diagnostics", response_model=LcpDiagnosticsResult)
def test_lcp_diagnostics(request: LcpHelloRequest) -> LcpDiagnosticsResult:
    result = run_lcp_diagnostic_snapshot(request, _load_station())
    _save_report(report_writer.save_lcp_diagnostics, result)
    return result


@app.post("/api/tests/lcp/active-rs485", response_model=LcpActiveRs485Result)
def test_lcp_active_rs485(request: LcpHelloRequest) -> LcpActiveRs485Result:
    result = run_lcp_active_rs485(request, _load_station())
    _save_report(report_writer.save_lcp_active_rs485, result)
    return result


@app.post("/api/tests/lcp/active-ethernet", response_model=LcpActiveEthernetResult)
def test_lcp_active_ethernet(request: LcpHelloRequest) -> LcpActiveEthernetResult:
    result = run_lcp_active_ethernet(request, _load_station())
    _save_report(report_writer.save_lcp_active_ethernet, result)
    return result


@app.post("/api/tests/lcp/active-services", response_model=LcpActiveServicesResult)
def test_lcp_active_services(request: LcpHelloRequest) -> LcpActiveServicesResult:
    result = run_lcp_active_services(request, _load_station())
    _save_report(report_writer.save_lcp_active_services, result)
    return result


@app.post("/api/tests/lcp/hmi", response_model=LcpHmiResult)
def test_lcp_hmi(request: LcpHelloRequest) -> LcpHmiResult:
    result = run_lcp_hmi_echo(request, _load_station())
    _save_report(report_writer.save_lcp_hmi, result)
    return result


@app.post("/api/tests/lcp/flash-ab", response_model=LcpFlashAbResult)
def test_lcp_flash_ab(request: ConfirmedTestRequest) -> LcpFlashAbResult:
    result = run_lcp_flash_ab(request, _load_station())
    _save_report(report_writer.save_lcp_flash_ab, result)
    return result


@app.post("/api/tests/lcp/watchdog-reset", response_model=LcpWatchdogResetResult)
def test_lcp_watchdog_reset(request: ConfirmedTestRequest) -> LcpWatchdogResetResult:
    result = run_lcp_watchdog_reset(request, _load_station())
    _save_report(report_writer.save_lcp_watchdog, result)
    return result


@app.post("/api/tests/lcp/rtc-retention/prepare", response_model=LcpRtcRetentionResult)
def test_rtc_retention_prepare(request: ConfirmedTestRequest) -> LcpRtcRetentionResult:
    result = prepare_rtc_retention(request, _load_station())
    _save_report(report_writer.save_lcp_rtc_retention, result)
    return result


@app.post("/api/tests/lcp/rtc-retention/verify", response_model=LcpRtcRetentionResult)
def test_rtc_retention_verify(request: ConfirmedTestRequest) -> LcpRtcRetentionResult:
    result = verify_rtc_retention(request, _load_station())
    _save_report(report_writer.save_lcp_rtc_retention, result)
    return result


@app.get("/api/station", response_model=StationConfig)
def get_station() -> StationConfig:
    return _load_station()


@app.put("/api/station", response_model=StationConfig)
def put_station(config: StationConfig) -> StationConfig:
    try:
        return station_store.save(config)
    except RuntimeError as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc


app.mount("/static", StaticFiles(directory=frontend_dir()), name="static")


@app.get("/", include_in_schema=False)
def index() -> FileResponse:
    return FileResponse(frontend_dir() / "index.html")


def run() -> None:
    url = f"http://{APP_HOST}:{APP_PORT}"
    threading.Timer(0.8, lambda: webbrowser.open(url)).start()
    uvicorn.run(app, host=APP_HOST, port=APP_PORT, log_level="info")


if __name__ == "__main__":
    run()
