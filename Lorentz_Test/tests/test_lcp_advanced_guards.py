import json
from datetime import datetime
from pathlib import Path

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import ConfirmedTestRequest, TestStatus
from lorentz_test.services import lcp_rtc_retention
from lorentz_test.services.lcp_rtc_retention import (
    prepare_rtc_retention,
    verify_rtc_retention,
)
from lorentz_test.services.lcp_watchdog_reset import run_lcp_watchdog_reset


def request(confirmation: str) -> ConfirmedTestRequest:
    return ConfirmedTestRequest(
        serial_number="LCP-1",
        operator="Operator",
        port="COM10",
        confirmation=confirmation,
    )


def test_watchdog_reset_requires_exact_confirmation() -> None:
    result = run_lcp_watchdog_reset(request("wrong"), StationConfig(lcp_port="COM10"))
    assert result.result == TestStatus.SKIPPED


def test_rtc_retention_prepare_requires_exact_confirmation() -> None:
    result = prepare_rtc_retention(request("wrong"), StationConfig(lcp_port="COM10"))
    assert result.result == TestStatus.SKIPPED


def test_rtc_retention_verify_before_interval_is_pending(
    tmp_path: Path,
    monkeypatch,
) -> None:
    state_path = tmp_path / "rtc_retention_state.json"
    prepared = datetime.now().replace(microsecond=0)
    state_path.write_text(
        json.dumps(
            {
                "serial_number": "LCP-1",
                "prepared_at_local": prepared.isoformat(),
                "rtc_at_prepare": prepared.isoformat(),
                "before_boot_count": 1,
                "battery_state": "ok",
                "port": "COM10",
            }
        ),
        encoding="utf-8",
    )
    monkeypatch.setattr(lcp_rtc_retention, "_state_path", lambda: state_path)

    result = verify_rtc_retention(
        request("RTC RETENTION VERIFY"),
        StationConfig(lcp_port="COM10"),
    )

    assert result.result == TestStatus.SKIPPED
    assert result.elapsed_pc_seconds is not None
    assert result.elapsed_pc_seconds < 30
    assert state_path.exists()
