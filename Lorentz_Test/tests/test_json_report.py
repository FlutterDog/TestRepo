from datetime import datetime, timezone
from pathlib import Path

from lorentz_test.models.tests import (
    LcpActiveRs485Result,
    LcpDiagnosticsResult,
    LcpHelloResult,
    TestStatus as Status,
)
from lorentz_test.reporting.json_report import JsonReportWriter


def test_json_report_is_saved_atomically(tmp_path: Path) -> None:
    result = LcpHelloResult(
        result=Status.PASS,
        started_at=datetime(2026, 7, 29, 18, 0, tzinfo=timezone.utc),
        duration_ms=12,
        station_name="Station A",
        serial_number="LCP/001",
        operator="Operator",
        port="COM7",
    )
    path = JsonReportWriter(tmp_path).save_lcp_hello(result)
    assert path.name == "LCP2116_LCP_001_20260729_180000_PASS.json"
    assert path.exists()
    assert result.report_file == str(path.resolve())
    text = path.read_text(encoding="utf-8")
    assert '"result": "PASS"' in text
    assert '"serial_number": "LCP/001"' in text
    assert not path.with_suffix(".json.tmp").exists()


def test_diagnostic_report_has_distinct_filename(tmp_path: Path) -> None:
    result = LcpDiagnosticsResult(
        result=Status.PASS,
        started_at=datetime(2026, 7, 30, 14, 0, tzinfo=timezone.utc),
        duration_ms=100,
        station_name="Station A",
        serial_number="LCP/001",
        operator="Operator",
        port="COM7",
    )
    path = JsonReportWriter(tmp_path).save_lcp_diagnostics(result)
    assert path.name == "LCP2116_LCP_001_20260730_140000_DIAGNOSTICS_PASS.json"
    assert path.exists()
    assert '"evaluation_mode": "semantic_v1.2"' in path.read_text(encoding="utf-8")


def test_active_rs485_report_has_fixture_suffix(tmp_path: Path) -> None:
    result = LcpActiveRs485Result(
        result=Status.FIXTURE_ERROR,
        started_at=datetime(2026, 7, 30, 15, 0, tzinfo=timezone.utc),
        duration_ms=50,
        station_name="Station A",
        serial_number="LCP/001",
        operator="Operator",
        port="COM7",
    )
    path = JsonReportWriter(tmp_path).save_lcp_active_rs485(result)
    assert path.name == "LCP2116_LCP_001_20260730_150000_ACTIVE_RS485_FIXTURE_ERROR.json"
    assert path.exists()
