from datetime import datetime, timezone
from pathlib import Path

from lorentz_test.models.full_test import FullTestStatus
from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import (
    ActiveInterfaceResult,
    ActiveStepResult,
    EthernetInterfaceResult,
    LcpActiveEthernetResult,
    LcpActiveRs485Result,
    LcpActiveServicesResult,
    LcpDiagnosticsResult,
    LcpHelloRequest,
    LcpHelloResult,
    LcpHmiResult,
    TestStatus as Status,
)
from lorentz_test.reporting.json_report import JsonReportWriter
from lorentz_test.services.lcp_full_test import run_lcp_full_test


NOW = datetime(2026, 7, 31, 6, 0, tzinfo=timezone.utc)


class FakeWriter:
    def __init__(self) -> None:
        self.calls: list[str] = []

    def _save(self, name: str, result: object) -> None:
        self.calls.append(name)
        result.report_file = f"C:/reports/{name}.json"

    def save_lcp_hello(self, result: object) -> None:
        self._save("hello", result)

    def save_lcp_diagnostics(self, result: object) -> None:
        self._save("diagnostics", result)

    def save_lcp_active_rs485(self, result: object) -> None:
        self._save("rs485", result)

    def save_lcp_active_ethernet(self, result: object) -> None:
        self._save("ethernet", result)

    def save_lcp_active_services(self, result: object) -> None:
        self._save("services", result)

    def save_lcp_hmi(self, result: object) -> None:
        self._save("hmi", result)


def station() -> StationConfig:
    return StationConfig(
        lcp_port="COM10",
        s1_endpoint="COM1",
        s2_endpoint="COM2",
        s3_endpoint="COM3",
        s4_endpoint="COM4",
        hmi_endpoint="COM5",
        x2x_endpoint="COM6",
        shared_hmi_x2x_adapter=False,
        eth1_test_enabled=True,
        eth2_test_enabled=True,
    )


def hello(status: Status = Status.PASS) -> LcpHelloResult:
    return LcpHelloResult(
        result=status,
        started_at=NOW,
        duration_ms=10,
        station_name="Station",
        serial_number="LCP-1",
        operator="Operator",
        port="COM10",
        error="USB failed" if status == Status.FAIL else None,
    )


def diagnostics() -> LcpDiagnosticsResult:
    return LcpDiagnosticsResult(
        result=Status.PASS,
        started_at=NOW,
        duration_ms=20,
        station_name="Station",
        serial_number="LCP-1",
        operator="Operator",
        port="COM10",
    )


def rs485_all_pass() -> LcpActiveRs485Result:
    interfaces = [
        ActiveInterfaceResult(
            name=name,
            endpoint=f"COM{index}",
            role="slave",
            status=Status.PASS,
            detail="verified",
        )
        for index, name in enumerate(("S1", "S2", "S3", "S4", "X2X"), start=1)
    ]
    return LcpActiveRs485Result(
        result=Status.PASS,
        started_at=NOW,
        duration_ms=30,
        station_name="Station",
        serial_number="LCP-1",
        operator="Operator",
        port="COM10",
        interfaces=interfaces,
    )


def ethernet_all_pass() -> LcpActiveEthernetResult:
    return LcpActiveEthernetResult(
        result=Status.PASS,
        started_at=NOW,
        duration_ms=40,
        station_name="Station",
        serial_number="LCP-1",
        operator="Operator",
        port="COM10",
        interfaces=[
            EthernetInterfaceResult(
                name="ETH1",
                target_ip="192.168.1.16",
                status=Status.PASS,
                detail="verified",
            ),
            EthernetInterfaceResult(
                name="ETH2",
                target_ip="192.168.1.1",
                status=Status.PASS,
                detail="verified",
            ),
        ],
    )


def services_all_pass() -> LcpActiveServicesResult:
    return LcpActiveServicesResult(
        result=Status.PASS,
        started_at=NOW,
        duration_ms=50,
        station_name="Station",
        serial_number="LCP-1",
        operator="Operator",
        port="COM10",
        steps=[
            ActiveStepResult(
                name="USB configuration read/validate",
                status=Status.PASS,
                duration_ms=1,
                detail="verified",
            ),
            ActiveStepResult(
                name="microSD write/read",
                status=Status.PASS,
                duration_ms=1,
                detail="verified",
            ),
            ActiveStepResult(
                name="RTC synchronization and tick",
                status=Status.PASS,
                duration_ms=1,
                detail="verified",
            ),
            ActiveStepResult(
                name="RTOS short stability",
                status=Status.PASS,
                duration_ms=1,
                detail="verified",
            ),
        ],
    )


def hmi_pass() -> LcpHmiResult:
    return LcpHmiResult(
        result=Status.PASS,
        started_at=NOW,
        duration_ms=60,
        station_name="Station",
        serial_number="LCP-1",
        operator="Operator",
        port="COM10",
        endpoint="COM5",
        detail="verified",
    )


def test_full_test_runs_on_backend_in_fixed_order_and_aggregates_pass() -> None:
    calls: list[str] = []

    def runner(name: str, value: object):
        def run(*_: object) -> object:
            calls.append(name)
            return value
        return run

    writer = FakeWriter()
    result = run_lcp_full_test(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM10"),
        station(),
        hello_runner=runner("hello", hello()),
        diagnostics_runner=runner("diagnostics", diagnostics()),
        rs485_runner=runner("rs485", rs485_all_pass()),
        ethernet_runner=runner("ethernet", ethernet_all_pass()),
        services_runner=runner("services", services_all_pass()),
        hmi_runner=runner("hmi", hmi_pass()),
        writer=writer,
    )

    assert calls == ["hello", "diagnostics", "rs485", "ethernet", "services", "hmi"]
    assert writer.calls == calls
    assert result.result == FullTestStatus.PASS
    assert result.summary.total_points == 14
    assert result.summary.evaluated_points == 14
    assert result.summary.passed_points == 14
    assert result.summary.pending_points == []
    assert all(stage.report_file for stage in result.stages)


def test_full_test_reports_fixture_error_and_pending_hardware() -> None:
    rs485 = rs485_all_pass()
    rs485.interfaces[0].status = Status.FIXTURE_ERROR
    rs485.interfaces[0].detail = "wrong fixture mapping"
    rs485.interfaces[2].status = Status.SKIPPED
    rs485.interfaces[3].status = Status.SKIPPED
    rs485.result = Status.FIXTURE_ERROR

    ethernet = ethernet_all_pass()
    ethernet.interfaces[0].status = Status.FIXTURE_ERROR
    ethernet.interfaces[0].detail = "link down"
    ethernet.result = Status.FIXTURE_ERROR

    hmi = hmi_pass()
    hmi.result = Status.SKIPPED
    hmi.detail = "endpoint not configured"

    result = run_lcp_full_test(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM10"),
        station(),
        hello_runner=lambda *_: hello(),
        diagnostics_runner=lambda *_: diagnostics(),
        rs485_runner=lambda *_: rs485,
        ethernet_runner=lambda *_: ethernet,
        services_runner=lambda *_: services_all_pass(),
        hmi_runner=lambda *_: hmi,
        writer=FakeWriter(),
    )

    assert result.result == FullTestStatus.FIXTURE_ERROR
    assert result.summary.fixture_error_points == 2
    assert result.summary.skipped_points == 3
    assert "S1" in result.summary.pending_points
    assert "ETH1" in result.summary.pending_points
    assert "HMI echo" in result.summary.pending_points


def test_usb_failure_blocks_dependent_hardware_stages() -> None:
    def must_not_run(*_: object) -> object:
        raise AssertionError("dependent runner must not be called")

    result = run_lcp_full_test(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM10"),
        station(),
        hello_runner=lambda *_: hello(Status.FAIL),
        diagnostics_runner=must_not_run,
        rs485_runner=must_not_run,
        ethernet_runner=must_not_run,
        services_runner=must_not_run,
        hmi_runner=must_not_run,
        writer=FakeWriter(),
    )

    assert result.result == FullTestStatus.FAIL
    assert result.stages[0].effective_result == FullTestStatus.FAIL
    assert all(stage.blocked_by == "USB и firmware" for stage in result.stages[1:])
    assert result.summary.failed_points == 1
    assert result.summary.skipped_points == 13


def test_full_test_json_has_aggregate_suffix(tmp_path: Path) -> None:
    writer = FakeWriter()
    result = run_lcp_full_test(
        LcpHelloRequest(serial_number="LCP/1", operator="Operator", port="COM10"),
        station(),
        hello_runner=lambda *_: hello(),
        diagnostics_runner=lambda *_: diagnostics(),
        rs485_runner=lambda *_: rs485_all_pass(),
        ethernet_runner=lambda *_: ethernet_all_pass(),
        services_runner=lambda *_: services_all_pass(),
        hmi_runner=lambda *_: hmi_pass(),
        writer=writer,
    )
    result.started_at = NOW

    path = JsonReportWriter(tmp_path).save_lcp_full_test(result)

    assert path.name == "LCP2116_LCP_1_20260731_060000_FULL_TEST_PASS.json"
    assert path.exists()
    text = path.read_text(encoding="utf-8")
    assert '"test_id": "lcp_full_sequential"' in text
    assert '"total_points": 14' in text
