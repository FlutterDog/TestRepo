"""FastAPI entry point for the local Lorentz Test utility."""

from __future__ import annotations

import threading
import webbrowser

import uvicorn
from fastapi import FastAPI
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from lorentz_test import __version__
from lorentz_test.paths import frontend_dir

APP_HOST = "127.0.0.1"
APP_PORT = 8765

app = FastAPI(title="Lorentz Test", version=__version__)
app.mount("/static", StaticFiles(directory=frontend_dir()), name="static")


@app.get("/api/health")
def health() -> dict[str, str]:
    return {"status": "ok", "version": __version__}


@app.get("/", include_in_schema=False)
def index() -> FileResponse:
    return FileResponse(frontend_dir() / "index.html")


def run() -> None:
    url = f"http://{APP_HOST}:{APP_PORT}"
    threading.Timer(0.8, lambda: webbrowser.open(url)).start()
    uvicorn.run(app, host=APP_HOST, port=APP_PORT, log_level="info")


if __name__ == "__main__":
    run()
