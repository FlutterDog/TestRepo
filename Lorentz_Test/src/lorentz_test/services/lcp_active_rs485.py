"""Active RS-485 fixture test with host-side Modbus RTU slave emulators."""

from __future__ import annotations

import re
import time
from collections.abc import Callable
from datetime import datetime, timezone

import serial

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import (
    ActiveInterfaceResult,
    LcpActiveRs485Result,
    LcpHelloRequest,
    TestStatus,
)
from lorentz_test.protocols.lcp_console import LcpDiagnosticConsole
from lorentz_test.protocols.lcp_diagnostic_parser import DiagnosticReport
from lorentz_test.protocols.modbus_rtu_slave import RtuSlaveServer

ConsoleOpener = Callable[..., LcpDiagnosticConsole]
ServerFactory = Callable[..., RtuSlaveServer]
_SLEEP_SECONDS = 4.0
_SERIAL_PATTERN = re.compile(r"^(?P<baud>\d+)\s+(?P<bits>[5-8])(?P<parity>[NEO])(?P<stop>[12])$")
_FIELD_TEST_VALUES: tuple[list[int], ...] = (
    [0x1101, 0x1102],
    [0x2201, 0x2202],
    [0x3301, 0x3302],
    [0x4401, 0x4402],
)


def _int(value: str | None, default: int = 0) -> int:
    try:
        return int(value or default)
    except (TypeError, ValueError):
        return default


def _serial_settings(text: str | None) -> tuple[int, str, float]:
    match = _SERIAL_PATTERN.match((text or "").strip().upper())
    if match is None or match.group("bits") != "8":
        raise ValueError(f"unsupported serial format: {text!r}")
    parity = {
        "N": serial.PARITY_NONE,
        "E": serial.PARITY_EVEN,
        "O": serial.PARITY_ODD,
    }[match.group("parity")]
    stopbits = serial.STOPBITS_ONE if match.group("stop") == "1" else serial.STOPBITS_TWO
    return int(match.group("baud")), parity, stopbits


def _fixture_error(
    name: str,
    endpoint: str | None,
    role: str,
    serial_text: str | None,
    exc: Exception,
) -> ActiveInterfaceResult:
    return ActiveInterfaceResult(
        name=name,
        endpoint=endpoint,
        role=role,
        serial=serial_text,
        status=TestStatus.FIXTURE_ERROR,
        detail=(
            "Ошибка стенда: Python не смог захватить endpoint и запустить slave; "
            f"{type(exc).__name__}: {exc}"
        ),
    )


def _skipped(
    name: str,
    endpoint: str | None,
    role: str,
    detail: str,
) -> ActiveInterfaceResult:
    return ActiveInterfaceResult(
        name=name,
        endpoint=endpoint,
        role=role,
        status=TestStatus.SKIPPED,
        detail=detail,
    )


def _field_provider(values: list[int]) -> Callable[[int, int], list[int] | None]:
    def provider(address: int, count: int) -> list[int] | None:
        if address == 0 and count == len(values):
            return list(values)
        return None

    return provider


def _x2x_lct1114_2_provider() -> Callable[[int, int], list[int] | None]:
    registers = [0] * 94
    registers[0] = 0x0005
    registers[1] = 0x0002
    registers[92] = 0x1357
    registers[93] = 0x2468

    def provider(address: int, count: int) -> list[int] | None:
        if address == 850 and count == 1:
            return [0]
        if 0 <= address < len(registers) and address + count <= len(registers):
            return registers[address : address + count]
        return None

    return provider


def _request_text(server: RtuSlaveServer) -> list[str]:
    return [
        f"FC{item.function:02X}@{item.address}+{item.count_or_value}"
        for item in server.stats.requests
    ]


def _copy_fixture_stats(item: ActiveInterfaceResult, server: RtuSlaveServer) -> None:
    stats = server.stats
    item.requests_received = stats.requests_received
    item.responses_sent = stats.responses_sent
    item.crc_errors = stats.crc_errors
    item.protocol_errors = stats.protocol_errors
    item.io_errors = stats.io_errors
    item.fixture_last_error = stats.last_error
    item.observed_requests = _request_text(server)


def _field_runtime(report: DiagnosticReport, name: str) -> dict[str, object]:
    return {
        "connection": report.one("connection", group=name),
        "valid": report.one("valid", group=name),
        "last_result": report.one("last_result", group=name),
        "success": _int(report.one("success", group=name)),
        "values": [
            _int(report.one("register[0]", group=name)),
            _int(report.one("register[1]", group=name)),
        ],
    }


def _classify_field_interfaces(
    interfaces: list[ActiveInterfaceResult],
    servers: dict[str, RtuSlaveServer],
    field_after: DiagnosticReport,
) -> None:
    """Classify DUT failures separately from fixture mapping and wiring errors."""
    runtime_by_port = {
        f"S{index}": _field_runtime(field_after, f"S{index}")
        for index in range(1, 5)
    }
    marker_seen_on: dict[tuple[int, ...], str] = {}
    for name, runtime in runtime_by_port.items():
        if runtime["connection"] == "online" and runtime["valid"] == "yes":
            marker_seen_on[tuple(runtime["values"])] = name  # type: ignore[arg-type]

    for item in interfaces:
        if not item.name.startswith("S") or item.status != TestStatus.RUNNING:
            continue

        server = servers[item.name]
        _copy_fixture_stats(item, server)
        stats = server.stats
        runtime = runtime_by_port[item.name]
        connection = str(runtime["connection"])
        valid = str(runtime["valid"])
        last_result = str(runtime["last_result"])
        after_success = int(runtime["success"])
        actual = list(runtime["values"])  # type: ignore[arg-type]
        item.after_success = after_success
        item.actual_values = actual

        passed = (
            stats.requests_received > 0
            and stats.responses_sent > 0
            and stats.io_errors == 0
            and connection == "online"
            and valid == "yes"
            and last_result == "ok"
            and after_success > (item.before_success or 0)
            and actual == item.expected_values
        )
        if passed:
            item.status = TestStatus.PASS
            item.detail = (
                f"connection={connection}, valid={valid}, result={last_result}, "
                f"success {item.before_success}->{after_success}, values={actual}; "
                f"fixture requests={stats.requests_received}, responses={stats.responses_sent}"
            )
            continue

        marker_target = marker_seen_on.get(tuple(item.expected_values))
        if marker_target is not None and marker_target != item.name:
            item.status = TestStatus.FIXTURE_ERROR
            item.detail = (
                f"Перекрёстная привязка стенда: endpoint {item.endpoint} настроен как "
                f"{item.name}, но его тестовые значения {item.expected_values} принял "
                f"LCP-порт {marker_target}. Переназначьте этот endpoint на {marker_target}. "
                f"fixture requests={stats.requests_received}, responses={stats.responses_sent}"
            )
            continue

        if stats.io_errors > 0 or stats.last_error:
            item.status = TestStatus.FIXTURE_ERROR
            item.detail = (
                f"Ошибка I/O стенда на {item.endpoint}: io_errors={stats.io_errors}, "
                f"last_error={stats.last_error or 'none'}"
            )
            continue

        if stats.requests_received == 0:
            item.status = TestStatus.FIXTURE_ERROR
            item.detail = (
                f"На endpoint {item.endpoint} не получено ни одного запроса от LCP. "
                "Проверьте соответствие COM-порта, кабель, A/B и канал serial server; "
                "DUT по этому соединению пока не оценивался."
            )
            continue

        if stats.responses_sent == 0:
            item.status = TestStatus.FIXTURE_ERROR
            item.detail = (
                f"Endpoint {item.endpoint} получил {stats.requests_received} запросов, "
                "но стенд не отправил ни одного ответа; DUT не оценивается."
            )
            continue

        item.status = TestStatus.FAIL
        item.detail = (
            f"DUT path failure: endpoint {item.endpoint} получил запросы и отправил ответы, "
            f"но {item.name} не подтвердил свои данные. connection={connection}, valid={valid}, "
            f"result={last_result}, success {item.before_success}->{after_success}, "
            f"values={actual}; fixture requests={stats.requests_received}, "
            f"responses={stats.responses_sent}"
        )


def _classify_x2x_interface(
    item: ActiveInterfaceResult,
    server: RtuSlaveServer,
    x2x_after: DiagnosticReport,
) -> None:
    _copy_fixture_stats(item, server)
    stats = server.stats
    groups = x2x_after.group_names("MODULE ")
    group = groups[0] if groups else ""
    connection = x2x_after.one("connection", group=group)
    error = x2x_after.one("communication_error", group=group)
    consecutive = _int(x2x_after.one("consecutive_failures", group=group), 255)
    after_success = _int(x2x_after.one("success", group=group))
    addresses = {request.address for request in stats.requests if request.function == 0x03}
    item.after_success = after_success

    if stats.io_errors > 0 or stats.last_error:
        item.status = TestStatus.FIXTURE_ERROR
        item.detail = (
            f"Ошибка I/O X2X fixture: io_errors={stats.io_errors}, "
            f"last_error={stats.last_error or 'none'}"
        )
        return

    if stats.requests_received == 0:
        item.status = TestStatus.FIXTURE_ERROR
        item.detail = (
            f"На X2X endpoint {item.endpoint} не получено запросов. Проверьте COM-порт, "
            "кабель, A/B и отсутствие другой программы; DUT по этой линии не оценивался."
        )
        return

    passed = (
        stats.requests_received >= 7
        and stats.responses_sent >= 7
        and 0 in addresses
        and 850 in addresses
        and connection == "online"
        and error == "ok"
        and consecutive == 0
        and after_success > (item.before_success or 0)
    )
    item.status = TestStatus.PASS if passed else TestStatus.FAIL
    item.detail = (
        f"connection={connection}, error={error}, consecutive_failures={consecutive}, "
        f"success {item.before_success}->{after_success}; fixture requests="
        f"{stats.requests_received}, responses={stats.responses_sent}, "
        f"FC03 addresses={sorted(addresses)}"
    )


def _overall_status(interfaces: list[ActiveInterfaceResult]) -> TestStatus:
    if any(item.status == TestStatus.FAIL for item in interfaces):
        return TestStatus.FAIL
    if any(item.status == TestStatus.FIXTURE_ERROR for item in interfaces):
        return TestStatus.FIXTURE_ERROR
    if any(item.status == TestStatus.PASS for item in interfaces):
        return TestStatus.PASS
    return TestStatus.SKIPPED


def run_lcp_active_rs485(
    request: LcpHelloRequest,
    station: StationConfig,
    *,
    console_opener: ConsoleOpener = LcpDiagnosticConsole.open_port,
    server_factory: ServerFactory = RtuSlaveServer,
    sleep: Callable[[float], None] = time.sleep,
) -> LcpActiveRs485Result:
    """Run S1-S4 and one configured LCT1114_2 X2X emulator without Modbus Poll."""
    port = request.port or station.lcp_port
    started_at = datetime.now(timezone.utc)
    started = time.monotonic()
    if not port:
        return LcpActiveRs485Result(
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
    servers: dict[str, RtuSlaveServer] = {}
    interfaces: list[ActiveInterfaceResult] = []
    field_before_raw: str | None = None
    field_after_raw: str | None = None
    x2x_before_raw: str | None = None
    x2x_after_raw: str | None = None

    try:
        console = console_opener(
            port,
            baudrate=station.serial_baudrate,
            timeout=max(3.0, station.serial_timeout_seconds),
        )
        field_before_raw = console.execute("field")
        rs485_raw = console.execute("rs485")
        x2x_before_raw = console.execute("x2x")
        field_before = DiagnosticReport(field_before_raw)
        rs485 = DiagnosticReport(rs485_raw)
        x2x_before = DiagnosticReport(x2x_before_raw)

        field_endpoints = (
            station.s1_endpoint,
            station.s2_endpoint,
            station.s3_endpoint,
            station.s4_endpoint,
        )
        for index, (endpoint, values) in enumerate(
            zip(field_endpoints, _FIELD_TEST_VALUES),
            start=1,
        ):
            name = f"S{index}"
            serial_text = field_before.one("serial", group=name)
            slave = _int(field_before.one("request_slave", group=name), 1)
            before_success = _int(field_before.one("success", group=name))
            if not endpoint:
                interfaces.append(
                    _skipped(name, None, "FieldSensor slave", "endpoint стенда не настроен")
                )
                continue
            try:
                baudrate, parity, stopbits = _serial_settings(serial_text)
                server = server_factory(
                    port=endpoint,
                    baudrate=baudrate,
                    parity=parity,
                    stopbits=stopbits,
                    slave_address=slave,
                    register_provider=_field_provider(values),
                )
                server.start()
                servers[name] = server
                interfaces.append(
                    ActiveInterfaceResult(
                        name=name,
                        endpoint=endpoint,
                        role="FieldSensor slave",
                        serial=serial_text,
                        status=TestStatus.RUNNING,
                        before_success=before_success,
                        expected_values=values,
                        detail="Python Modbus RTU slave запущен",
                    )
                )
            except Exception as exc:
                interfaces.append(
                    _fixture_error(name, endpoint, "FieldSensor slave", serial_text, exc)
                )

        module_groups = x2x_before.group_names("MODULE ")
        if not station.x2x_endpoint:
            interfaces.append(
                _skipped("X2X", None, "LCT1114_2 emulator", "X2X endpoint стенда не настроен")
            )
        elif len(module_groups) != 1:
            interfaces.append(
                _skipped(
                    "X2X",
                    station.x2x_endpoint,
                    "LCT1114_2 emulator",
                    f"релиз поддерживает ровно один configured module; найдено {len(module_groups)}",
                )
            )
        else:
            group = module_groups[0]
            type_name = x2x_before.one("type_name", group=group)
            slave = _int(x2x_before.one("slave", group=group), 1)
            serial_text = rs485.one("serial", group="X2X physical port")
            before_success = _int(x2x_before.one("success", group=group))
            if type_name != "LCT1114_2":
                interfaces.append(
                    _skipped(
                        "X2X",
                        station.x2x_endpoint,
                        "X2X module emulator",
                        f"тип {type_name or 'missing'} ещё не поддержан активным эмулятором",
                    )
                )
            else:
                try:
                    baudrate, parity, stopbits = _serial_settings(serial_text)
                    server = server_factory(
                        port=station.x2x_endpoint,
                        baudrate=baudrate,
                        parity=parity,
                        stopbits=stopbits,
                        slave_address=slave,
                        register_provider=_x2x_lct1114_2_provider(),
                    )
                    server.start()
                    servers["X2X"] = server
                    interfaces.append(
                        ActiveInterfaceResult(
                            name="X2X",
                            endpoint=station.x2x_endpoint,
                            role="LCT1114_2 emulator",
                            serial=serial_text,
                            status=TestStatus.RUNNING,
                            before_success=before_success,
                            detail=f"Python X2X slave {slave} запущен",
                        )
                    )
                except Exception as exc:
                    interfaces.append(
                        _fixture_error(
                            "X2X",
                            station.x2x_endpoint,
                            "LCT1114_2 emulator",
                            serial_text,
                            exc,
                        )
                    )

        if servers:
            sleep(_SLEEP_SECONDS)
        field_after_raw = console.execute("field")
        x2x_after_raw = console.execute("x2x")
        field_after = DiagnosticReport(field_after_raw)
        x2x_after = DiagnosticReport(x2x_after_raw)

        _classify_field_interfaces(interfaces, servers, field_after)
        for item in interfaces:
            if item.name == "X2X" and item.status == TestStatus.RUNNING:
                _classify_x2x_interface(item, servers[item.name], x2x_after)

        return LcpActiveRs485Result(
            result=_overall_status(interfaces),
            started_at=started_at,
            duration_ms=round((time.monotonic() - started) * 1000),
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port=port,
            interfaces=interfaces,
            field_before_raw=field_before_raw,
            field_after_raw=field_after_raw,
            x2x_before_raw=x2x_before_raw,
            x2x_after_raw=x2x_after_raw,
        )
    except Exception as exc:
        return LcpActiveRs485Result(
            result=TestStatus.FAIL,
            started_at=started_at,
            duration_ms=round((time.monotonic() - started) * 1000),
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port=port,
            interfaces=interfaces,
            field_before_raw=field_before_raw,
            field_after_raw=field_after_raw,
            x2x_before_raw=x2x_before_raw,
            x2x_after_raw=x2x_after_raw,
            error=f"{type(exc).__name__}: {exc}",
        )
    finally:
        for server in servers.values():
            server.stop()
        if console is not None:
            console.close()
