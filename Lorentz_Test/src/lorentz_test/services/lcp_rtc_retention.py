"""Two-phase manual power-cycle test for RTC backup retention."""

from __future__ import annotations

import json
import os
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import (
    ActiveStepResult,
    CheckResult,
    ConfirmedTestRequest,
    LcpRtcRetentionResult,
    TestStatus,
)
from lorentz_test.paths import data_dir
from lorentz_test.protocols.lcp_console import LcpDiagnosticConsole
from lorentz_test.protocols.lcp_diagnostic_parser import DiagnosticReport


_STATE_FILE = "rtc_retention_state.json"


def _state_path() -> Path:
    return data_dir() / _STATE_FILE


def _int(value: str | None, default: int = 0) -> int:
    try:
        return int(value or default, 0)
    except (TypeError, ValueError):
        return default


def _datetime(raw: str, group: str | None = None) -> datetime | None:
    value = DiagnosticReport(raw).one("datetime", group=group)
    if not value or value == "unavailable":
        return None
    try:
        return datetime.strptime(value, "%Y-%m-%d %H:%M:%S")
    except ValueError:
        return None


def _check(name: str, expected: str, actual: object, passed: bool) -> CheckResult:
    return CheckResult(
        name=name,
        expected=expected,
        actual=str(actual),
        status=TestStatus.PASS if passed else TestStatus.FAIL,
    )


def _base_result(
    request: ConfirmedTestRequest,
    station: StationConfig,
    phase: str,
    started_at: datetime,
) -> LcpRtcRetentionResult:
    return LcpRtcRetentionResult(
        phase=phase,
        result=TestStatus.RUNNING,
        started_at=started_at,
        duration_ms=0,
        station_name=station.station_name,
        serial_number=request.serial_number,
        operator=request.operator,
        port=request.port or station.lcp_port or "",
        state_file=str(_state_path().resolve()),
    )


def prepare_rtc_retention(
    request: ConfirmedTestRequest,
    station: StationConfig,
) -> LcpRtcRetentionResult:
    """Synchronize RTC and persist the baseline before a complete power removal."""
    started_at = datetime.now(timezone.utc)
    started = time.monotonic()
    result = _base_result(request, station, "PREPARE", started_at)
    if request.confirmation != "RTC RETENTION PREPARE":
        result.result = TestStatus.SKIPPED
        result.error = "confirmation must be exactly RTC RETENTION PREPARE"
        return result
    if not result.port:
        result.result = TestStatus.FIXTURE_ERROR
        result.error = "LCP USB port is not configured"
        return result

    console: LcpDiagnosticConsole | None = None
    try:
        console = LcpDiagnosticConsole.open_port(
            result.port,
            baudrate=station.serial_baudrate,
            timeout=max(4.0, station.serial_timeout_seconds),
        )
        watchdog_raw = console.execute("watchdog")
        before_boot = _int(DiagnosticReport(watchdog_raw).one("boot_count", group="Last reset"))
        requested = datetime.now().replace(microsecond=0) + timedelta(seconds=1)
        set_raw = console.execute(f"rtc set {requested:%Y-%m-%d %H:%M:%S}")
        set_result = DiagnosticReport(set_raw).one("update_start_result")
        time.sleep(0.8)
        rtc_raw = console.execute("rtc")
        rtc_at_prepare = _datetime(rtc_raw, "Current date and time")
        battery_raw = console.execute("battery")
        battery = DiagnosticReport(battery_raw).one("debounced_state", section=None)
        if rtc_at_prepare is None:
            raise RuntimeError("RTC did not return a valid date/time after synchronization")

        prepared_local = datetime.now().replace(microsecond=0)
        state = {
            "serial_number": request.serial_number,
            "prepared_at_local": prepared_local.isoformat(),
            "rtc_at_prepare": rtc_at_prepare.isoformat(),
            "before_boot_count": before_boot,
            "battery_state": battery,
            "port": result.port,
        }
        path = _state_path()
        temporary = path.with_suffix(path.suffix + ".tmp")
        temporary.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
        os.replace(temporary, path)

        checks = [
            _check("rtc_update_start", "ok", set_result or "missing", set_result == "ok"),
            _check("rtc_read_back", "valid", rtc_at_prepare, True),
            _check("battery_comparator", "ok", battery or "missing", battery == "ok"),
        ]
        passed = all(item.status == TestStatus.PASS for item in checks)
        result.result = TestStatus.PASS if passed else TestStatus.FAIL
        result.prepared_at = prepared_local
        result.before_boot_count = before_boot
        result.battery_state = battery
        result.steps.append(
            ActiveStepResult(
                name="Prepare RTC retention baseline",
                status=result.result,
                duration_ms=round((time.monotonic() - started) * 1000),
                detail=(
                    "Baseline saved. Now disconnect USB and all main power, wait at least "
                    "30 seconds, then restore power and run VERIFY."
                ),
                checks=checks,
                raw_action=set_raw,
                raw_before=watchdog_raw,
                raw_after=rtc_raw + battery_raw,
            )
        )
    except Exception as exc:
        result.result = TestStatus.FAIL
        result.error = f"{type(exc).__name__}: {exc}"
    finally:
        if console is not None:
            console.close()
        result.duration_ms = round((time.monotonic() - started) * 1000)
    return result


def verify_rtc_retention(
    request: ConfirmedTestRequest,
    station: StationConfig,
) -> LcpRtcRetentionResult:
    """Compare elapsed RTC and PC time after the operator restores DUT power."""
    started_at = datetime.now(timezone.utc)
    started = time.monotonic()
    result = _base_result(request, station, "VERIFY", started_at)
    if request.confirmation != "RTC RETENTION VERIFY":
        result.result = TestStatus.SKIPPED
        result.error = "confirmation must be exactly RTC RETENTION VERIFY"
        return result

    path = _state_path()
    if not path.exists():
        result.result = TestStatus.FIXTURE_ERROR
        result.error = "no RTC retention PREPARE state exists"
        return result
    if not result.port:
        result.result = TestStatus.FIXTURE_ERROR
        result.error = "LCP USB port is not configured"
        return result

    console: LcpDiagnosticConsole | None = None
    try:
        state = json.loads(path.read_text(encoding="utf-8"))
        if state.get("serial_number") != request.serial_number:
            raise RuntimeError(
                f"retention state belongs to {state.get('serial_number')}, not {request.serial_number}"
            )
        prepared_at = datetime.fromisoformat(state["prepared_at_local"])
        rtc_at_prepare = datetime.fromisoformat(state["rtc_at_prepare"])
        before_boot = int(state["before_boot_count"])

        console = LcpDiagnosticConsole.open_port(
            result.port,
            baudrate=station.serial_baudrate,
            timeout=max(4.0, station.serial_timeout_seconds),
        )
        rtc_raw = console.execute("rtc")
        rtc_now = _datetime(rtc_raw, "Current date and time")
        watchdog_raw = console.execute("watchdog")
        after_boot = _int(DiagnosticReport(watchdog_raw).one("boot_count", group="Last reset"))
        battery_raw = console.execute("battery")
        battery = DiagnosticReport(battery_raw).one("debounced_state")
        if rtc_now is None:
            raise RuntimeError("RTC is unreadable after power restoration")

        now = datetime.now().replace(microsecond=0)
        elapsed_pc = (now - prepared_at).total_seconds()
        elapsed_rtc = (rtc_now - rtc_at_prepare).total_seconds()
        error_seconds = abs(elapsed_rtc - elapsed_pc)
        checks = [
            _check("minimum_power_off_interval", ">= 30 s", f"{elapsed_pc:.0f} s", elapsed_pc >= 30),
            _check("boot_count_increment", f"> {before_boot}", after_boot, after_boot > before_boot),
            _check("rtc_elapsed_time", "difference from PC <= 10 s", f"error={error_seconds:.1f} s", error_seconds <= 10),
            _check("battery_comparator", "ok", battery or "missing", battery == "ok"),
        ]
        passed = all(item.status == TestStatus.PASS for item in checks)
        result.result = TestStatus.PASS if passed else TestStatus.FAIL
        result.prepared_at = prepared_at
        result.elapsed_pc_seconds = elapsed_pc
        result.elapsed_rtc_seconds = elapsed_rtc
        result.retention_error_seconds = error_seconds
        result.before_boot_count = before_boot
        result.after_boot_count = after_boot
        result.battery_state = battery
        result.steps.append(
            ActiveStepResult(
                name="Verify RTC backup retention",
                status=result.result,
                duration_ms=round((time.monotonic() - started) * 1000),
                detail=(
                    f"PC elapsed={elapsed_pc:.1f}s, RTC elapsed={elapsed_rtc:.1f}s, "
                    f"error={error_seconds:.1f}s, boot_count {before_boot}->{after_boot}"
                ),
                checks=checks,
                raw_before=rtc_raw,
                raw_after=watchdog_raw + battery_raw,
            )
        )
        if passed:
            path.unlink(missing_ok=True)
    except Exception as exc:
        result.result = TestStatus.FAIL
        result.error = f"{type(exc).__name__}: {exc}"
    finally:
        if console is not None:
            console.close()
        result.duration_ms = round((time.monotonic() - started) * 1000)
    return result
