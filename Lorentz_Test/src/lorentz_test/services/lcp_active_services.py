"""Safe active tests for configuration transport, microSD, RTC and RTOS stability."""

from __future__ import annotations

import struct
import time
from collections.abc import Callable
from datetime import datetime, timedelta, timezone

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import (
    ActiveStepResult,
    CheckResult,
    LcpActiveServicesResult,
    LcpHelloRequest,
    TestStatus,
)
from lorentz_test.protocols.lcp_console import (
    LcpDiagnosticConsole,
    parse_key_value_output,
)
from lorentz_test.protocols.lcp_diagnostic_parser import DiagnosticReport
from lorentz_test.protocols.lcp_usb import (
    COMMAND_GET_CONFIG,
    COMMAND_VALIDATE_CONFIG,
    STATUS_OK,
    LcpUsbClient,
    ProtocolError,
    SerialTransportError,
    crc32,
)

ConsoleOpener = Callable[..., LcpDiagnosticConsole]
ClientFactory = Callable[..., LcpUsbClient]


def _int(value: str | None, default: int = 0) -> int:
    try:
        return int(value or default, 0)
    except (TypeError, ValueError):
        return default


def _percent(value: str | None) -> float:
    if not value:
        return 0.0
    token = value.split("(")[-1].split("%", 1)[0].strip()
    try:
        return float(token)
    except ValueError:
        return 0.0


def _check(name: str, expected: str, actual: object, passed: bool) -> CheckResult:
    return CheckResult(
        name=name,
        expected=expected,
        actual=str(actual),
        status=TestStatus.PASS if passed else TestStatus.FAIL,
    )


def _overall_status(steps: list[ActiveStepResult]) -> TestStatus:
    if any(step.status == TestStatus.FAIL for step in steps):
        return TestStatus.FAIL
    if any(step.status == TestStatus.FIXTURE_ERROR for step in steps):
        return TestStatus.FIXTURE_ERROR
    if any(step.status == TestStatus.PASS for step in steps):
        return TestStatus.PASS
    return TestStatus.SKIPPED


def _parse_rtc_datetime(raw: str, *, full_report: bool) -> datetime | None:
    if full_report:
        value = DiagnosticReport(raw).one("datetime", group="Current date and time")
    else:
        value = DiagnosticReport(raw).one("datetime", section="RTC TIME")
    if not value or value == "unavailable":
        return None
    try:
        return datetime.strptime(value, "%Y-%m-%d %H:%M:%S")
    except ValueError:
        return None


def _run_config_validation(
    port: str,
    station: StationConfig,
    client_factory: ClientFactory,
) -> ActiveStepResult:
    started = time.monotonic()
    try:
        with client_factory(
            port,
            baudrate=station.serial_baudrate,
            timeout=station.serial_timeout_seconds,
        ) as client:
            hello = client.hello()
            first = client.request(COMMAND_GET_CONFIG, accepted={STATUS_OK})
            if len(first.payload) != 124:
                raise ProtocolError(f"GET_CONFIG length {len(first.payload)} != 124")
            schema, bundle_size = struct.unpack_from("<HH", first.payload, 0)
            sequence, generation, stored_crc = struct.unpack_from("<III", first.payload, 4)
            source = first.payload[16]
            slot = first.payload[17]
            bundle = first.payload[20:]
            calculated_crc = crc32(bundle)
            validate_payload = struct.pack("<HHI", schema, bundle_size, calculated_crc) + bundle
            client.request(
                COMMAND_VALIDATE_CONFIG,
                validate_payload,
                accepted={STATUS_OK},
            )
            second = client.request(COMMAND_GET_CONFIG, accepted={STATUS_OK})

        checks = [
            _check("hello_schema", "1 / bundle 104", f"{hello.schema_version} / {hello.bundle_size}", hello.schema_version == 1 and hello.bundle_size == 104),
            _check("get_config_length", "124", len(first.payload), len(first.payload) == 124),
            _check("bundle_schema", "1 / 104", f"{schema} / {bundle_size}", schema == 1 and bundle_size == 104),
            _check("bundle_crc", f"0x{stored_crc:08X}", f"0x{calculated_crc:08X}", stored_crc == calculated_crc),
            _check("validate_config", "STATUS_OK", "STATUS_OK", True),
            _check("read_back_unchanged", "identical GET_CONFIG payload", "identical" if second.payload == first.payload else "changed", second.payload == first.payload),
        ]
        passed = all(item.status == TestStatus.PASS for item in checks)
        return ActiveStepResult(
            name="USB configuration read/validate",
            status=TestStatus.PASS if passed else TestStatus.FAIL,
            duration_ms=round((time.monotonic() - started) * 1000),
            detail=(
                f"source={source}, slot={slot}, sequence={sequence}, generation={generation}, "
                f"crc32=0x{stored_crc:08X}; no Flash write was issued"
            ),
            checks=checks,
        )
    except SerialTransportError as exc:
        return ActiveStepResult(
            name="USB configuration read/validate",
            status=TestStatus.FIXTURE_ERROR,
            duration_ms=round((time.monotonic() - started) * 1000),
            detail=f"cannot access LCP USB fixture: {exc}",
        )
    except Exception as exc:
        return ActiveStepResult(
            name="USB configuration read/validate",
            status=TestStatus.FAIL,
            duration_ms=round((time.monotonic() - started) * 1000),
            detail=f"{type(exc).__name__}: {exc}",
        )


def _run_sd(console: LcpDiagnosticConsole) -> ActiveStepResult:
    started = time.monotonic()
    try:
        before = console.execute("sd")
        action = console.execute("sd test")
        after = console.execute("sd")
        values = parse_key_value_output(action)
        after_values = DiagnosticReport(after)
        checks = [
            _check("sd_write", "ok", values.get("write_result", "missing"), values.get("write_result", "").casefold() == "ok"),
            _check("sd_read", "ok", values.get("read_result", "missing"), values.get("read_result", "").casefold() == "ok"),
            _check("sd_loaded_count", "3", values.get("loaded_count", "missing"), values.get("loaded_count") == "3"),
            _check("sd_data_match", "yes", values.get("data_match", "missing"), values.get("data_match", "").casefold() == "yes"),
            _check("sd_result", "OK", values.get("result", "missing"), values.get("result", "").casefold() == "ok"),
            _check("sd_file_exists_after", "yes", after_values.one("sdtest.txt_exists", group="Filesystem") or "missing", after_values.one("sdtest.txt_exists", group="Filesystem") == "yes"),
        ]
        passed = all(item.status == TestStatus.PASS for item in checks)
        return ActiveStepResult(
            name="microSD write/read",
            status=TestStatus.PASS if passed else TestStatus.FAIL,
            duration_ms=round((time.monotonic() - started) * 1000),
            detail="firmware overwrote and read back only SDTEST.TXT; configuration files were not modified",
            checks=checks,
            raw_before=before,
            raw_action=action,
            raw_after=after,
        )
    except Exception as exc:
        return ActiveStepResult(
            name="microSD write/read",
            status=TestStatus.FAIL,
            duration_ms=round((time.monotonic() - started) * 1000),
            detail=f"{type(exc).__name__}: {exc}",
        )


def _run_rtc(console: LcpDiagnosticConsole, sleep: Callable[[float], None]) -> ActiveStepResult:
    started = time.monotonic()
    try:
        requested = datetime.now().replace(microsecond=0) + timedelta(seconds=1)
        command = f"rtc set {requested:%Y-%m-%d %H:%M:%S}"
        action = console.execute(command)
        action_values = parse_key_value_output(action)
        sleep(0.6)
        first_raw = console.execute("rtc")
        first_report = DiagnosticReport(first_raw)
        first_time = _parse_rtc_datetime(first_raw, full_report=True)
        sleep(3.0)
        second_raw = console.execute("time")
        second_time = _parse_rtc_datetime(second_raw, full_report=False)
        now = datetime.now().replace(microsecond=0)

        start_ok = action_values.get("rtc update_start_result", "").casefold() == "ok"
        update_idle = first_report.one("update_state", group="Clock and update state") == "idle"
        update_ok = first_report.one("update_result", group="Clock and update state") == "ok"
        first_delta = abs((first_time - requested).total_seconds()) if first_time else 10**9
        advance = (second_time - first_time).total_seconds() if first_time and second_time else -1
        pc_delta = abs((second_time - now).total_seconds()) if second_time else 10**9
        checks = [
            _check("rtc_update_start", "ok", action_values.get("rtc update_start_result", "missing"), start_ok),
            _check("rtc_update_complete", "state=idle, result=ok", f"state={first_report.one('update_state', group='Clock and update state')}, result={first_report.one('update_result', group='Clock and update state')}", update_idle and update_ok),
            _check("rtc_set_read_back", "difference <= 3 s", f"difference={first_delta:.0f} s", first_delta <= 3),
            _check("rtc_advances", "2..6 s", f"advance={advance:.0f} s", 2 <= advance <= 6),
            _check("rtc_matches_pc", "difference <= 5 s", f"difference={pc_delta:.0f} s", pc_delta <= 5),
        ]
        passed = all(item.status == TestStatus.PASS for item in checks)
        return ActiveStepResult(
            name="RTC synchronization and tick",
            status=TestStatus.PASS if passed else TestStatus.FAIL,
            duration_ms=round((time.monotonic() - started) * 1000),
            detail=f"requested={requested}; first={first_time}; second={second_time}",
            checks=checks,
            raw_action=action,
            raw_before=first_raw,
            raw_after=second_raw,
        )
    except Exception as exc:
        return ActiveStepResult(
            name="RTC synchronization and tick",
            status=TestStatus.FAIL,
            duration_ms=round((time.monotonic() - started) * 1000),
            detail=f"{type(exc).__name__}: {exc}",
        )


def _run_rtos(console: LcpDiagnosticConsole, sleep: Callable[[float], None]) -> ActiveStepResult:
    started = time.monotonic()
    try:
        before_raw = console.execute("rtos")
        sleep(2.0)
        after_raw = console.execute("rtos")
        before = DiagnosticReport(before_raw)
        after = DiagnosticReport(after_raw)
        tick_before = _int(before.one("tick_count", group="Runtime"))
        tick_after = _int(after.one("tick_count", group="Runtime"))
        uptime_before = _int(before.one("uptime_ms", group="Runtime"))
        uptime_after = _int(after.one("uptime_ms", group="Runtime"))
        heap_percent = _percent(after.one("minimum_ever_free_bytes", group="FreeRTOS heap_4"))
        stack_percent = _percent(after.one("minimum_free_bytes", group="LCP task stack"))
        checks = [
            _check("rtos_scheduler", "running", after.one("scheduler", group="Runtime") or "missing", after.one("scheduler", group="Runtime") == "running"),
            _check("rtos_tick_progress", "> 0", tick_after - tick_before, tick_after > tick_before),
            _check("rtos_uptime_progress", ">= 1500 ms", uptime_after - uptime_before, uptime_after - uptime_before >= 1500),
            _check("heap_minimum_free", ">= 10%", f"{heap_percent:.1f}%", heap_percent >= 10.0),
            _check("stack_minimum_free", ">= 10%", f"{stack_percent:.1f}%", stack_percent >= 10.0),
        ]
        passed = all(item.status == TestStatus.PASS for item in checks)
        return ActiveStepResult(
            name="RTOS short stability",
            status=TestStatus.PASS if passed else TestStatus.FAIL,
            duration_ms=round((time.monotonic() - started) * 1000),
            detail="two RTOS snapshots with a two-second observation interval",
            checks=checks,
            raw_before=before_raw,
            raw_after=after_raw,
        )
    except Exception as exc:
        return ActiveStepResult(
            name="RTOS short stability",
            status=TestStatus.FAIL,
            duration_ms=round((time.monotonic() - started) * 1000),
            detail=f"{type(exc).__name__}: {exc}",
        )


def run_lcp_active_services(
    request: LcpHelloRequest,
    station: StationConfig,
    *,
    client_factory: ClientFactory = LcpUsbClient,
    console_opener: ConsoleOpener = LcpDiagnosticConsole.open_port,
    sleep: Callable[[float], None] = time.sleep,
) -> LcpActiveServicesResult:
    """Run all safe active internal-service tests without writing configuration Flash."""
    port = request.port or station.lcp_port
    started_at = datetime.now(timezone.utc)
    started = time.monotonic()
    if not port:
        return LcpActiveServicesResult(
            result=TestStatus.FAIL,
            started_at=started_at,
            duration_ms=0,
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port="",
            error="LCP USB port is not configured",
        )

    steps = [_run_config_validation(port, station, client_factory)]
    console: LcpDiagnosticConsole | None = None
    try:
        sleep(0.4)
        console = console_opener(
            port,
            baudrate=station.serial_baudrate,
            timeout=max(4.0, station.serial_timeout_seconds),
        )
        steps.append(_run_sd(console))
        steps.append(_run_rtc(console, sleep))
        steps.append(_run_rtos(console, sleep))
    except Exception as exc:
        steps.append(
            ActiveStepResult(
                name="USB diagnostic console",
                status=TestStatus.FIXTURE_ERROR,
                duration_ms=0,
                detail=f"{type(exc).__name__}: {exc}",
            )
        )
    finally:
        if console is not None:
            console.close()

    return LcpActiveServicesResult(
        result=_overall_status(steps),
        started_at=started_at,
        duration_ms=round((time.monotonic() - started) * 1000),
        station_name=station.station_name,
        serial_number=request.serial_number,
        operator=request.operator,
        port=port,
        steps=steps,
    )
