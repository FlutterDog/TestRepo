from lorentz_test.models.tests import CheckResult, TestStatus as Status
from lorentz_test.services.lcp_diagnostics import _passive_snapshot_checks


def test_passive_snapshot_skips_absolute_rtc_time_check() -> None:
    checks = [
        CheckResult(
            name="rtc_time_matches_pc",
            expected="difference <= 300 s",
            actual="2007-01-04 12:15:39; difference=617522669 s",
            status=Status.FAIL,
        )
    ]
    adjusted = _passive_snapshot_checks("rtc", checks)
    assert adjusted[0].status == Status.SKIPPED
    assert "active RTC synchronization" in adjusted[0].expected
    assert "passive snapshot only" in adjusted[0].actual


def test_passive_snapshot_skips_x2x_module_communication() -> None:
    checks = [
        CheckResult(
            name="x2x_module_1",
            expected="online",
            actual="connection=lost, error=timeout",
            status=Status.FAIL,
        )
    ]
    adjusted = _passive_snapshot_checks("x2x", checks)
    assert adjusted[0].status == Status.SKIPPED
    assert "host X2X slave emulator" in adjusted[0].expected
