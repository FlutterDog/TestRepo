from datetime import datetime, timezone
from pathlib import Path

from lorentz_test.models.tests import LcpHelloResult, TestStatus as Status
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
