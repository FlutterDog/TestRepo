from datetime import datetime, timezone

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpHelloRequest, LcpHmiResult, TestStatus as Status
from lorentz_test.services.lcp_full_test import run_hmi_for_full_test


def request() -> LcpHelloRequest:
    return LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM10")


def test_shared_same_endpoint_skips_automatic_hmi() -> None:
    called = False

    def runner(_request: LcpHelloRequest, _station: StationConfig) -> LcpHmiResult:
        nonlocal called
        called = True
        raise AssertionError("HMI runner must not be called for a shared movable adapter")

    station = StationConfig(
        lcp_port="COM10",
        hmi_endpoint="COM7",
        x2x_endpoint="com7",
        shared_hmi_x2x_adapter=True,
    )

    result = run_hmi_for_full_test(request(), station, runner)

    assert result.result == Status.SKIPPED
    assert result.endpoint == "COM7"
    assert "отдельный HMI" in result.detail
    assert called is False


def test_separate_hmi_endpoint_runs_automatic_hmi() -> None:
    called = False

    def runner(req: LcpHelloRequest, station: StationConfig) -> LcpHmiResult:
        nonlocal called
        called = True
        return LcpHmiResult(
            result=Status.PASS,
            started_at=datetime.now(timezone.utc),
            duration_ms=1,
            station_name=station.station_name,
            serial_number=req.serial_number,
            operator=req.operator,
            port=req.port or station.lcp_port or "",
            endpoint=station.hmi_endpoint,
            detail="verified",
        )

    station = StationConfig(
        lcp_port="COM10",
        hmi_endpoint="COM7",
        x2x_endpoint="COM6",
        shared_hmi_x2x_adapter=True,
    )

    result = run_hmi_for_full_test(request(), station, runner)

    assert called is True
    assert result.result == Status.PASS
