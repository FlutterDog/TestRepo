"""Controlled watchdog reset and USB recovery test."""

from __future__ import annotations

import time
from datetime import datetime, timezone

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import (
    ActiveStepResult,
    CheckResult,
    ConfirmedTestRequest,
    LcpWatchdogResetResult,
    TestStatus,
)
from lorentz_test.protocols.lcp_console import LcpDiagnosticConsole
from lorentz_test.protocols.lcp_diagnostic_parser import DiagnosticReport
from lorentz_test.protocols.lcp_reconnect import (
    capture_port_identity,
    wait_for_lcp_reconnect,
)


def _int(value: str | None, default: int = 0) -> int:
    try:
        return int(value or default, 0)
    except (TypeError, ValueError):
        return default


def _check(name: str, expected: str, actual: object, passed: bool) -> CheckResult:
    return CheckResult(
        name=name,
        expected=expected,
        actual=str(actual),
        status=TestStatus.PASS if passed else TestStatus.FAIL,
    )


def run_lcp_watchdog_reset(
    request: ConfirmedTestRequest,
    station: StationConfig,
) -> LcpWatchdogResetResult:
    """Stop watchdog feed, wait for hardware reset and verify recovered USB runtime."""
    port = request.port or station.lcp_port or ""
    started_at = datetime.now(timezone.utc)
    started = time.monotonic()
    result = LcpWatchdogResetResult(
        result=TestStatus.RUNNING,
        started_at=started_at,
        duration_ms=0,
        station_name=station.station_name,
        serial_number=request.serial_number,
        operator=request.operator,
        port=port,
    )
    if request.confirmation != "WATCHDOG RESET":
        result.result = TestStatus.SKIPPED
        result.error = "confirmation must be exactly WATCHDOG RESET"
        return result
    if not port:
        result.result = TestStatus.FIXTURE_ERROR
        result.error = "LCP USB port is not configured"
        return result

    console: LcpDiagnosticConsole | None = None
    identity = capture_port_identity(port)
    try:
        step_started = time.monotonic()
        console = LcpDiagnosticConsole.open_port(
            port,
            baudrate=station.serial_baudrate,
            timeout=max(4.0, station.serial_timeout_seconds),
        )
        before_raw = console.execute("watchdog")
        before = DiagnosticReport(before_raw)
        before_boot = _int(before.one("boot_count", group="Last reset"))
        enabled_before = before.one("enabled", group="Runtime")
        armed_before = before.one("test_reset_armed", group="Runtime")
        result.before_raw = before_raw
        result.before_boot_count = before_boot
        result.steps.append(
            ActiveStepResult(
                name="Verify watchdog runtime",
                status=(
                    TestStatus.PASS
                    if enabled_before == "yes" and armed_before == "no"
                    else TestStatus.FAIL
                ),
                duration_ms=round((time.monotonic() - step_started) * 1000),
                detail=(
                    f"enabled={enabled_before}, test_reset_armed={armed_before}, "
                    f"boot_count={before_boot}"
                ),
            )
        )
        if enabled_before != "yes" or armed_before != "no":
            raise RuntimeError("watchdog runtime is not in the safe pre-test state")

        step_started = time.monotonic()
        arm_raw = console.execute("watchdog test reset")
        result.arm_raw = arm_raw
        console.close()
        console = None
        arm_ok = (
            "watchdog reset test armed" in arm_raw.casefold()
            and "automatic feed stopped" in arm_raw.casefold()
        )
        result.steps.append(
            ActiveStepResult(
                name="Arm watchdog reset",
                status=TestStatus.PASS if arm_ok else TestStatus.FAIL,
                duration_ms=round((time.monotonic() - step_started) * 1000),
                detail="firmware stopped automatic watchdog feed",
                raw_action=arm_raw,
            )
        )
        if not arm_ok:
            raise RuntimeError("firmware did not acknowledge watchdog reset test")

        step_started = time.monotonic()
        time.sleep(5.0)
        reconnected = wait_for_lcp_reconnect(
            port,
            identity,
            baudrate=station.serial_baudrate,
            serial_timeout=station.serial_timeout_seconds,
            total_timeout=35.0,
        )
        result.reconnected_port = reconnected
        time.sleep(0.5)
        console = LcpDiagnosticConsole.open_port(
            reconnected,
            baudrate=station.serial_baudrate,
            timeout=max(4.0, station.serial_timeout_seconds),
        )
        after_raw = console.execute("watchdog")
        after = DiagnosticReport(after_raw)
        after_boot = _int(after.one("boot_count", group="Last reset"))
        reset_type = after.one("reset_type", group="Last reset")
        recovery = after.one("usb_recovery_after_watchdog", group="Runtime")
        enabled_after = after.one("enabled", group="Runtime")
        armed_after = after.one("test_reset_armed", group="Runtime")
        status_register = after.one("wdt_sr", group="Raw registers")
        result.after_raw = after_raw
        result.after_boot_count = after_boot
        result.reset_type = reset_type
        result.recovery_performed = recovery

        checks = [
            _check("usb_reconnect", "successful HELLO and console", reconnected, True),
            _check("boot_count_increment", f"> {before_boot}", after_boot, after_boot > before_boot),
            _check(
                "watchdog_recovery",
                "performed",
                recovery or "missing",
                recovery == "performed",
            ),
            _check(
                "reset_type",
                "software reset after watchdog recovery or watchdog reset",
                reset_type or "missing",
                reset_type in {"software reset", "watchdog reset"},
            ),
            _check("watchdog_reenabled", "yes", enabled_after or "missing", enabled_after == "yes"),
            _check("test_disarmed", "no", armed_after or "missing", armed_after == "no"),
            _check("wdt_status", "0x0", status_register or "missing", status_register == "0x0"),
        ]
        passed = all(item.status == TestStatus.PASS for item in checks)
        result.steps.append(
            ActiveStepResult(
                name="Verify reset and recovered runtime",
                status=TestStatus.PASS if passed else TestStatus.FAIL,
                duration_ms=round((time.monotonic() - step_started) * 1000),
                detail=(
                    f"port={reconnected}, boot_count {before_boot}->{after_boot}, "
                    f"reset_type={reset_type}, recovery={recovery}"
                ),
                checks=checks,
                raw_after=after_raw,
            )
        )
        result.result = TestStatus.PASS if passed else TestStatus.FAIL
    except Exception as exc:
        result.result = TestStatus.FAIL
        result.error = f"{type(exc).__name__}: {exc}"
    finally:
        if console is not None:
            console.close()
        result.duration_ms = round((time.monotonic() - started) * 1000)

    return result
