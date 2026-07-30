"""FastAPI entry point for the local Lorentz Test utility."""

from __future__ import annotations

import threading
import webbrowser

import uvicorn
from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from lorentz_test import __version__
from lorentz_test.models.serial import SerialPortInfo
from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpDiagnosticsResult, LcpHelloRequest, LcpHelloResult
from lorentz_test.paths import frontend_dir
from lorentz_test.reporting.json_report import report_writer
from lorentz_test.serial_ports import list_serial_ports
from lorentz_test.services.lcp_diagnostics import run_lcp_diagnostic_snapshot
from lorentz_test.services.lcp_hello import run_lcp_hello
from lorentz_test.station_store import station_store

APP_HOST = "127.0.0.1"
APP_PORT = 8765

app = FastAPI(title="Lorentz Test", version=__version__)


@app.get("/api/health")
def health() -> dict[str, str]:
    return {"status": "ok", "version": __version__}


@app.get("/api/ports", response_model=list[SerialPortInfo])
def get_ports() -> list[SerialPortInfo]:
    try:
        return list_serial_ports()
    except RuntimeError as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc


@app.post("/api/tests/lcp/hello", response_model=LcpHelloResult)
def test_lcp_hello(request: LcpHelloRequest) -> LcpHelloResult:
    try:
        station = station_store.load()
    except RuntimeError as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc
    result = run_lcp_hello(request, station)
    try:
        report_writer.save_lcp_hello(result)
    except RuntimeError as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc
    return result


@app.post("/api/tests/lcp/diagnostics", response_model=LcpDiagnosticsResult)
def test_lcp_diagnostics(request: LcpHelloRequest) -> LcpDiagnosticsResult:
    try:
        station = station_store.load()
    except RuntimeError as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc
    result = run_lcp_diagnostic_snapshot(request, station)
    try:
        report_writer.save_lcp_diagnostics(result)
    except RuntimeError as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc
    return result


@app.get("/api/station", response_model=StationConfig)
def get_station() -> StationConfig:
    try:
        return station_store.load()
    except RuntimeError as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc


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
