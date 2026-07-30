"""Active end-to-end test of the two LCP W5500 Modbus TCP servers."""

from __future__ import annotations

import time
from collections.abc import Callable
from datetime import datetime, timezone

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import (
    EthernetInterfaceResult,
    LcpActiveEthernetResult,
    LcpHelloRequest,
    TestStatus,
)
from lorentz_test.protocols.lcp_console import LcpDiagnosticConsole
from lorentz_test.protocols.lcp_diagnostic_parser import DiagnosticReport
from lorentz_test.protocols.modbus_tcp_client import (
    ModbusTcpProtocolError,
    ModbusTcpReadResult,
    ModbusTcpTransportError,
    read_holding_registers,
)

ConsoleOpener = Callable[..., LcpDiagnosticConsole]
TcpReader = Callable[..., ModbusTcpReadResult]


def _int(value: str | None, default: int = 0) -> int:
    try:
        return int(value or default, 0)
    except (TypeError, ValueError):
        return default


def _overall_status(items: list[EthernetInterfaceResult]) -> TestStatus:
    if any(item.status == TestStatus.FAIL for item in items):
        return TestStatus.FAIL
    if any(item.status == TestStatus.FIXTURE_ERROR for item in items):
        return TestStatus.FIXTURE_ERROR
    if any(item.status == TestStatus.PASS for item in items):
        return TestStatus.PASS
    return TestStatus.SKIPPED


def run_lcp_active_ethernet(
    request: LcpHelloRequest,
    station: StationConfig,
    *,
    console_opener: ConsoleOpener = LcpDiagnosticConsole.open_port,
    tcp_reader: TcpReader = read_holding_registers,
) -> LcpActiveEthernetResult:
    """Read all twelve holding registers through each configured Ethernet port."""
    port = request.port or station.lcp_port
    started_at = datetime.now(timezone.utc)
    started = time.monotonic()
    if not port:
        return LcpActiveEthernetResult(
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
    items: list[EthernetInterfaceResult] = []
    before_raw: str | None = None
    after_raw: str | None = None
    successful_names: set[str] = set()

    try:
        console = console_opener(
            port,
            baudrate=station.serial_baudrate,
            timeout=max(3.0, station.serial_timeout_seconds),
        )
        before_raw = console.execute("eth")
        before = DiagnosticReport(before_raw)

        definitions = (
            (
                "ETH1",
                station.eth1_test_enabled,
                str(station.eth1_ip),
                str(station.eth1_source_ip) if station.eth1_source_ip else None,
                0x7101,
            ),
            (
                "ETH2",
                station.eth2_test_enabled,
                str(station.eth2_ip),
                str(station.eth2_source_ip) if station.eth2_source_ip else None,
                0x7102,
            ),
        )

        for name, enabled, expected_ip, source_ip, transaction_id in definitions:
            interface_started = time.monotonic()
            actual_ip = before.one("ip", group=name)
            link = before.one("link", group=name)
            item = EthernetInterfaceResult(
                name=name,
                target_ip=expected_ip,
                source_ip=source_ip,
                status=TestStatus.RUNNING,
                link_before=link,
                transaction_id=transaction_id,
                before_requests=_int(before.one("requests", group=name)),
                before_responses=_int(before.one("responses", group=name)),
                before_transport_errors=_int(before.one("transport_errors", group=name)),
                detail="active Modbus TCP test started",
            )
            items.append(item)

            if not enabled:
                item.status = TestStatus.SKIPPED
                item.detail = "active Ethernet test disabled in station settings"
                item.duration_ms = round((time.monotonic() - interface_started) * 1000)
                continue
            if not actual_ip:
                item.status = TestStatus.FAIL
                item.detail = "DUT diagnostics did not report an IP address"
                item.duration_ms = round((time.monotonic() - interface_started) * 1000)
                continue
            if actual_ip != expected_ip:
                item.status = TestStatus.FIXTURE_ERROR
                item.detail = (
                    f"station target IP {expected_ip} does not match DUT active IP {actual_ip}; "
                    "update station settings or controller configuration"
                )
                item.duration_ms = round((time.monotonic() - interface_started) * 1000)
                continue
            if link != "up":
                item.status = TestStatus.FIXTURE_ERROR
                item.detail = (
                    f"DUT reports link={link or 'unknown'}; connect the cable and configure "
                    "the PC network adapter before evaluating the DUT"
                )
                item.duration_ms = round((time.monotonic() - interface_started) * 1000)
                continue

            try:
                result = tcp_reader(
                    expected_ip,
                    start_address=0,
                    register_count=12,
                    source_ip=source_ip,
                    port=502,
                    unit_id=1,
                    transaction_id=transaction_id,
                    timeout=2.0,
                )
                item.socket_connected = True
                item.registers = result.registers
                successful_names.add(name)
            except ModbusTcpTransportError as exc:
                item.status = TestStatus.FIXTURE_ERROR
                item.detail = (
                    f"link is up but host could not reach {expected_ip}:502; "
                    f"check route/source adapter/firewall: {exc}"
                )
            except ModbusTcpProtocolError as exc:
                item.socket_connected = True
                item.status = TestStatus.FAIL
                item.detail = f"connected DUT returned invalid Modbus TCP response: {exc}"
            except Exception as exc:
                item.status = TestStatus.FIXTURE_ERROR
                item.detail = f"unexpected host-side Ethernet test error: {type(exc).__name__}: {exc}"
            item.duration_ms = round((time.monotonic() - interface_started) * 1000)

        after_raw = console.execute("eth")
        after = DiagnosticReport(after_raw)

        for item in items:
            item.link_after = after.one("link", group=item.name)
            item.after_requests = _int(after.one("requests", group=item.name))
            item.after_responses = _int(after.one("responses", group=item.name))
            item.after_transport_errors = _int(after.one("transport_errors", group=item.name))
            if item.name not in successful_names:
                continue

            passed = (
                item.link_after == "up"
                and len(item.registers) == 12
                and item.after_requests > (item.before_requests or 0)
                and item.after_responses > (item.before_responses or 0)
                and item.after_transport_errors == (item.before_transport_errors or 0)
            )
            item.status = TestStatus.PASS if passed else TestStatus.FAIL
            item.detail = (
                f"FC03 registers 0..11 read={item.registers}; link {item.link_before}->{item.link_after}; "
                f"requests {item.before_requests}->{item.after_requests}; "
                f"responses {item.before_responses}->{item.after_responses}; "
                f"transport_errors {item.before_transport_errors}->{item.after_transport_errors}"
            )

        return LcpActiveEthernetResult(
            result=_overall_status(items),
            started_at=started_at,
            duration_ms=round((time.monotonic() - started) * 1000),
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port=port,
            interfaces=items,
            eth_before_raw=before_raw,
            eth_after_raw=after_raw,
        )
    except Exception as exc:
        return LcpActiveEthernetResult(
            result=TestStatus.FAIL,
            started_at=started_at,
            duration_ms=round((time.monotonic() - started) * 1000),
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port=port,
            interfaces=items,
            eth_before_raw=before_raw,
            eth_after_raw=after_raw,
            error=f"{type(exc).__name__}: {exc}",
        )
    finally:
        if console is not None:
            console.close()
