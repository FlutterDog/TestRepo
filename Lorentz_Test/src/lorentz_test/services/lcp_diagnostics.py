"""Capture and evaluate non-destructive LCP diagnostic console reports."""

from __future__ import annotations

import dataclasses
import time
from collections.abc import Callable
from datetime import datetime, timezone

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import (
    DiagnosticCommandResult,
    DiagnosticParsedValue,
    LcpDiagnosticsResult,
    LcpHelloRequest,
    TestStatus,
)
from lorentz_test.protocols.lcp_console import LcpDiagnosticConsole
from lorentz_test.protocols.lcp_diagnostic_parser import (
    parse_diagnostic_output,
    qualified_values,
)
from lorentz_test.services.lcp_diagnostic_evaluation import (
    evaluate_diagnostic_command,
)

ConsoleOpener = Callable[..., LcpDiagnosticConsole]

_DIAGNOSTIC_COMMANDS: tuple[tuple[str, str, str], ...] = (
    ("rtos", "RTOS и память", "rtos"),
    ("config_status", "Конфигурация и Flash A/B", "config status"),
    ("field", "FieldSensor S1–S4", "field"),
    ("rs485", "Владельцы RS-485 и UART errors", "rs485"),
    ("sc16is", "Внешние UART SC16IS7xx", "sc16is"),
    ("x2x", "Внутренняя шина X2X", "x2x"),
    ("ethernet", "ETH1/ETH2 и Modbus TCP", "eth"),
    ("sd", "microSD", "sd"),
    ("battery", "Резервная батарея", "battery"),
    ("rtc", "RTC", "rtc"),
    ("watchdog", "Watchdog и reset cause", "watchdog"),
)


def _capture_command(
    console: LcpDiagnosticConsole,
    station: StationConfig,
    command_id: str,
    title: str,
    command: str,
) -> DiagnosticCommandResult:
    started = time.monotonic()
    try:
        raw_output = console.execute(command)
        lowered = raw_output.casefold()
        if "unknown command" in lowered or "command rejected" in lowered:
            raise RuntimeError("firmware rejected the diagnostic command")

        parsed_entries = parse_diagnostic_output(raw_output)
        status, checks = evaluate_diagnostic_command(
            command_id,
            raw_output,
            station,
        )
        return DiagnosticCommandResult(
            command_id=command_id,
            command=command,
            title=title,
            capture_status=TestStatus.PASS,
            status=status,
            duration_ms=round((time.monotonic() - started) * 1000),
            checks=checks,
            parsed_values=qualified_values(parsed_entries),
            parsed_entries=[
                DiagnosticParsedValue(**dataclasses.asdict(item))
                for item in parsed_entries
            ],
            raw_output=raw_output,
        )
    except Exception as exc:
        return DiagnosticCommandResult(
            command_id=command_id,
            command=command,
            title=title,
            capture_status=TestStatus.FAIL,
            status=TestStatus.FAIL,
            duration_ms=round((time.monotonic() - started) * 1000),
            error=f"{type(exc).__name__}: {exc}",
        )


def run_lcp_diagnostic_snapshot(
    request: LcpHelloRequest,
    station: StationConfig,
    *,
    console_opener: ConsoleOpener = LcpDiagnosticConsole.open_port,
) -> LcpDiagnosticsResult:
    """Capture reports and apply the semantic profile for LCP Basic 1.02.0."""
    port = request.port or station.lcp_port
    started_at = datetime.now(timezone.utc)
    started = time.monotonic()

    if not port:
        return LcpDiagnosticsResult(
            result=TestStatus.FAIL,
            started_at=started_at,
            duration_ms=0,
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port="",
            error="LCP USB port is not configured",
        )

    console: LcpDiagnosticConsole | None = None
    commands: list[DiagnosticCommandResult] = []
    try:
        console = console_opener(
            port,
            baudrate=station.serial_baudrate,
            timeout=max(3.0, station.serial_timeout_seconds),
        )
        for command_id, title, command in _DIAGNOSTIC_COMMANDS:
            commands.append(
                _capture_command(
                    console,
                    station,
                    command_id,
                    title,
                    command,
                )
            )

        result = (
            TestStatus.FAIL
            if any(item.status == TestStatus.FAIL for item in commands)
            else TestStatus.PASS
        )
        return LcpDiagnosticsResult(
            result=result,
            started_at=started_at,
            duration_ms=round((time.monotonic() - started) * 1000),
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port=port,
            commands=commands,
        )
    except Exception as exc:
        return LcpDiagnosticsResult(
            result=TestStatus.FAIL,
            started_at=started_at,
            duration_ms=round((time.monotonic() - started) * 1000),
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port=port,
            commands=commands,
            error=f"{type(exc).__name__}: {exc}",
        )
    finally:
        if console is not None:
            console.close()
