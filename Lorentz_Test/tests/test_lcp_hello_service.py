from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpHelloRequest, TestStatus as Status
from lorentz_test.protocols.lcp_console import FirmwareIdentity
from lorentz_test.protocols.lcp_usb import HelloInfo
from lorentz_test.services.lcp_hello import run_lcp_hello


class FakeClient:
    def __init__(self, port: str, **_: object) -> None:
        self.port = port
        self.closed = False
        self.exited = False
        self.transport = object()

    def hello(self) -> HelloInfo:
        return HelloInfo(1, 1, 104, 160, 0x0F, 1, 24)

    def exit_binary_session(self) -> None:
        self.exited = True

    def close(self) -> None:
        self.closed = True


class FakeConsole:
    version = "1.02.0"

    def __init__(self, _: object) -> None:
        pass

    def read_firmware_identity(self) -> FirmwareIdentity:
        return FirmwareIdentity(
            name="LCP Basic Diagnostic Firmware",
            version=self.version,
            stage=f"Release {self.version}",
            target="ATSAM3X8E",
            raw_output=(
                "name = LCP Basic Diagnostic Firmware\r\n"
                f"version = {self.version}\r\n"
                f"stage = Release {self.version}\r\n"
                "target = ATSAM3X8E\r\n"
            ),
        )


class WrongVersionConsole(FakeConsole):
    version = "1.01.0"


def test_lcp_identity_passes_reference_profile() -> None:
    result = run_lcp_hello(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM7"),
        StationConfig(),
        client_factory=FakeClient,
        console_factory=FakeConsole,
    )
    assert result.result == Status.PASS
    assert result.hello is not None
    assert result.firmware_version == "1.02.0"
    assert result.firmware_version_verified is True
    assert len(result.checks) == 11


def test_lcp_identity_fails_wrong_firmware_version() -> None:
    result = run_lcp_hello(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM7"),
        StationConfig(),
        client_factory=FakeClient,
        console_factory=WrongVersionConsole,
    )
    assert result.result == Status.FAIL
    assert result.firmware_version == "1.01.0"
    assert result.firmware_version_verified is False


def test_lcp_identity_requires_port() -> None:
    result = run_lcp_hello(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator"),
        StationConfig(lcp_port=None),
        client_factory=FakeClient,
        console_factory=FakeConsole,
    )
    assert result.result == Status.FAIL
    assert "not configured" in (result.error or "")
