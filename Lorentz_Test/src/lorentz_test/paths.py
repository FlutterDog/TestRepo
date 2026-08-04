"""Application path resolution."""

from __future__ import annotations

import os
from pathlib import Path


def project_root() -> Path:
    """Return the Lorentz_Test source root."""
    return Path(__file__).resolve().parents[2]


def frontend_dir() -> Path:
    return project_root() / "frontend"


def data_dir() -> Path:
    override = os.environ.get("LORENTZ_TEST_DATA_DIR")
    path = Path(override).expanduser().resolve() if override else project_root() / "runtime"
    path.mkdir(parents=True, exist_ok=True)
    return path


def reports_dir() -> Path:
    override = os.environ.get("LORENTZ_TEST_REPORT_DIR")
    path = Path(override).expanduser().resolve() if override else project_root() / "reports"
    path.mkdir(parents=True, exist_ok=True)
    return path
