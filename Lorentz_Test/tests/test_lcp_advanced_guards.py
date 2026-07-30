from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import ConfirmedTestRequest, TestStatus
from lorentz_test.services.lcp_rtc_retention import prepare_rtc_retention
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
