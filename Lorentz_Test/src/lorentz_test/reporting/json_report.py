"""Atomic JSON report writer."""

from __future__ import annotations

import os
import re
from pathlib import Path

from lorentz_test.models.tests import LcpHelloResult
from lorentz_test.paths import reports_dir

_SAFE_FILENAME = re.compile(r"[^A-Za-z0-9._-]+")


def _safe_component(value: str, fallback: str) -> str:
    cleaned = _SAFE_FILENAME.sub("_", value.strip()).strip("._-")
    return cleaned[:80] or fallback


class JsonReportWriter:
    def __init__(self, directory: Path | None = None) -> None:
        self.directory = directory or reports_dir()

    def save_lcp_hello(self, result: LcpHelloResult) -> Path:
        self.directory.mkdir(parents=True, exist_ok=True)
        serial_number = _safe_component(result.serial_number, "unknown_serial")
        timestamp = result.started_at.strftime("%Y%m%d_%H%M%S")
        filename = f"LCP2116_{serial_number}_{timestamp}_{result.result.value}.json"
        target = self.directory / filename
        temporary = target.with_suffix(target.suffix + ".tmp")

        result.report_file = str(target.resolve())
        try:
            temporary.write_text(result.model_dump_json(indent=2) + "\n", encoding="utf-8")
            os.replace(temporary, target)
        except OSError as exc:
            temporary.unlink(missing_ok=True)
            result.report_file = None
            raise RuntimeError(f"cannot save JSON report: {exc}") from exc
        return target


report_writer = JsonReportWriter()
