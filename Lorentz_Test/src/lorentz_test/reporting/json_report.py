"""Atomic JSON report writer."""

from __future__ import annotations

import os
import re
from pathlib import Path
from typing import Protocol

from lorentz_test.models.tests import (
    LcpActiveEthernetResult,
    LcpActiveRs485Result,
    LcpActiveServicesResult,
    LcpDiagnosticsResult,
    LcpFlashAbResult,
    LcpHelloResult,
    LcpHmiResult,
    LcpRtcRetentionResult,
    LcpWatchdogResetResult,
)
from lorentz_test.paths import reports_dir

_SAFE_FILENAME = re.compile(r"[^A-Za-z0-9._-]+")


class ReportModel(Protocol):
    serial_number: str
    started_at: object
    result: object
    report_file: str | None

    def model_dump_json(self, *, indent: int) -> str: ...


def _safe_component(value: str, fallback: str) -> str:
    cleaned = _SAFE_FILENAME.sub("_", value.strip()).strip("._-")
    return cleaned[:80] or fallback


class JsonReportWriter:
    def __init__(self, directory: Path | None = None) -> None:
        self.directory = directory or reports_dir()

    def _save(self, result: ReportModel, suffix: str = "") -> Path:
        self.directory.mkdir(parents=True, exist_ok=True)
        serial_number = _safe_component(result.serial_number, "unknown_serial")
        timestamp = result.started_at.strftime("%Y%m%d_%H%M%S")
        status = result.result.value
        middle = f"_{suffix}" if suffix else ""
        filename = f"LCP2116_{serial_number}_{timestamp}{middle}_{status}.json"
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

    def save_lcp_hello(self, result: LcpHelloResult) -> Path:
        return self._save(result)

    def save_lcp_diagnostics(self, result: LcpDiagnosticsResult) -> Path:
        return self._save(result, "DIAGNOSTICS")

    def save_lcp_active_rs485(self, result: LcpActiveRs485Result) -> Path:
        return self._save(result, "ACTIVE_RS485")

    def save_lcp_active_ethernet(self, result: LcpActiveEthernetResult) -> Path:
        return self._save(result, "ACTIVE_ETHERNET")

    def save_lcp_active_services(self, result: LcpActiveServicesResult) -> Path:
        return self._save(result, "ACTIVE_SERVICES")

    def save_lcp_hmi(self, result: LcpHmiResult) -> Path:
        return self._save(result, "HMI_ECHO")

    def save_lcp_flash_ab(self, result: LcpFlashAbResult) -> Path:
        return self._save(result, "FLASH_AB")

    def save_lcp_watchdog(self, result: LcpWatchdogResetResult) -> Path:
        return self._save(result, "WATCHDOG_RESET")

    def save_lcp_rtc_retention(self, result: LcpRtcRetentionResult) -> Path:
        return self._save(result, f"RTC_RETENTION_{result.phase}")


report_writer = JsonReportWriter()
