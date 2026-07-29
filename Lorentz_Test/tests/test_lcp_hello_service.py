from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpHelloRequest, TestStatus as Status
from lorentz_test.protocols.lcp_usb import HelloInfo
from lorentz_test.services.lcp_hello import run_lcp_hello


class FakeClient:
    def __init__(self, port: str, **_: object) -> None:
        self.port = port
        self.closed = False

    def hello(self) -> HelloInfo:
        return HelloInfo(1, 1, 104, 160, 0x0F, 1, 24)

    def close(self) -> None:
        self.closed = True


def test_lcp_hello_passes_reference_profile() -> None:
    result = run_lcp_hello(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM7"),
        StationConfig(),
        client_factory=FakeClient,
    )
    assert result.result == Status.PASS
    assert result.hello is not None
    assert result.firmware_version_verified is False


def test_lcp_hello_requires_port() -> None:
    result = run_lcp_hello(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator"),
        StationConfig(lcp_port=None),
        client_factory=FakeClient,
    )
    assert result.result == Status.FAIL
    assert "not configured" in (result.error or "")
