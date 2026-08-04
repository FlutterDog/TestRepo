from __future__ import annotations

from datetime import datetime, timezone

from lorentz_test.models.precheck import PrecheckCheckStatus, PrecheckStatus
from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import (
    EndpointAccessResult,
    EndpointAccessStatus,
    LcpHelloRequest,
    LcpHelloResult,
    TestStatus as LcpTestStatus,
)
from lorentz_test.services.lcp_precheck import run_lcp_precheck


class DummySocket:
    def close(self) -> None:
        pass


def _request() -> LcpHelloRequest:
    return LcpHelloRequest(serial_number="LCP_TEST", operator="Tester", port="COM10")


def _hello(result: LcpTestStatus = LcpTestStatus.PASS) -> LcpHelloResult:
    return LcpHelloResult(
        result=result,
        started_at=datetime.now(timezone.utc),
        duration_ms=1,
        station_name="Station",
        serial_number="LCP_TEST",
        operator="Tester",
        port="COM10",
        firmware_version="1.02.0" if result == LcpTestStatus.PASS else None,
        error=None if result == LcpTestStatus.PASS else "USB unavailable",
    )


def test_precheck_ready_with_optional_interfaces_skipped(tmp_path) -> None:
    reports = tmp_path / "reports"
    runtime = tmp_path / "runtime"
    reports.mkdir()
    runtime.mkdir()
    station = StationConfig(
        lcp_port="COM10",
        s1_endpoint="COM5",
        hmi_endpoint="COM4",
        x2x_endpoint="COM6",
        eth1_ip="192.168.1.16",
        eth2_test_enabled=False,
        shared_hmi_x2x_adapter=False,
    )

    endpoints = [
        EndpointAccessResult(
            name="S1",
            endpoint="COM5",
            status=EndpointAccessStatus.AVAILABLE,
            detail="available",
        ),
        EndpointAccessResult(
            name="S2",
            endpoint=None,
            status=EndpointAccessStatus.SKIPPED,
            detail="not configured",
        ),
        EndpointAccessResult(
            name="HMI",
            endpoint="COM4",
            status=EndpointAccessStatus.AVAILABLE,
            detail="available",
        ),
        EndpointAccessResult(
            name="X2X adapter",
            endpoint="COM6",
            status=EndpointAccessStatus.AVAILABLE,
            detail="available",
        ),
    ]

    result = run_lcp_precheck(
        _request(),
        station,
        hello_runner=lambda request, config: _hello(),
        endpoint_probe=lambda config: endpoints,
        tcp_connector=lambda target, **kwargs: DummySocket(),
        reports_path_factory=lambda: reports,
        runtime_path_factory=lambda: runtime,
    )

    assert result.result == PrecheckStatus.READY
    assert result.ready is True
    assert result.blocking_failures == []
    assert "Endpoint S2" in result.warnings
    assert any(item.key == "eth1" and item.status == PrecheckCheckStatus.PASS for item in result.checks)
    assert any(item.key == "eth2" and item.status == PrecheckCheckStatus.SKIPPED for item in result.checks)


def test_precheck_blocks_on_usb_busy_fixture_and_ethernet(tmp_path) -> None:
    reports = tmp_path / "reports"
    runtime = tmp_path / "runtime"
    reports.mkdir()
    runtime.mkdir()
    station = StationConfig(
        lcp_port="COM10",
        s1_endpoint="COM5",
        eth1_ip="192.168.1.16",
        eth2_test_enabled=False,
        shared_hmi_x2x_adapter=False,
    )
    endpoints = [
        EndpointAccessResult(
            name="S1",
            endpoint="COM5",
            status=EndpointAccessStatus.BUSY,
            detail="port busy",
        )
    ]

    def failed_connect(target, **kwargs):
        raise OSError("network unreachable")

    result = run_lcp_precheck(
        _request(),
        station,
        hello_runner=lambda request, config: _hello(LcpTestStatus.FAIL),
        endpoint_probe=lambda config: endpoints,
        tcp_connector=failed_connect,
        reports_path_factory=lambda: reports,
        runtime_path_factory=lambda: runtime,
    )

    assert result.result == PrecheckStatus.NOT_READY
    assert result.ready is False
    assert result.blocking_failures == ["USB и firmware", "Endpoint S1", "ETH1"]


def test_precheck_warns_when_shared_flag_does_not_match_endpoints(tmp_path) -> None:
    reports = tmp_path / "reports"
    runtime = tmp_path / "runtime"
    reports.mkdir()
    runtime.mkdir()
    station = StationConfig(
        lcp_port="COM10",
        hmi_endpoint="COM4",
        x2x_endpoint="COM6",
        eth1_test_enabled=False,
        eth2_test_enabled=False,
        shared_hmi_x2x_adapter=True,
    )

    result = run_lcp_precheck(
        _request(),
        station,
        hello_runner=lambda request, config: _hello(),
        endpoint_probe=lambda config: [],
        tcp_connector=lambda target, **kwargs: DummySocket(),
        reports_path_factory=lambda: reports,
        runtime_path_factory=lambda: runtime,
    )

    shared = next(item for item in result.checks if item.key == "shared_hmi_x2x")
    assert result.ready is True
    assert shared.status == PrecheckCheckStatus.WARNING
    assert "выключите этот флаг" in shared.detail
