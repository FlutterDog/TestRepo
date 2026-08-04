from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpHelloRequest, TestStatus
from lorentz_test.services.lcp_hmi import run_lcp_hmi_echo


class EchoTransport:
    def __init__(self) -> None:
        self.buffer = bytearray()
        self.closed = False

    def reset_input_buffer(self) -> None:
        self.buffer.clear()

    def reset_output_buffer(self) -> None:
        pass

    def write(self, data: bytes) -> int:
        self.buffer.extend(data)
        return len(data)

    def flush(self) -> None:
        pass

    def read(self, size: int) -> bytes:
        chunk = bytes(self.buffer[:size])
        del self.buffer[:size]
        return chunk

    def close(self) -> None:
        self.closed = True


def test_hmi_echo_passes_three_exact_frames() -> None:
    transport = EchoTransport()

    def factory(*_: object, **__: object) -> EchoTransport:
        return transport

    result = run_lcp_hmi_echo(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM10"),
        StationConfig(lcp_port="COM10", hmi_endpoint="COM20"),
        serial_factory=factory,
        sleep=lambda _: None,
    )

    assert result.result == TestStatus.PASS
    assert result.frames_sent == 3
    assert result.frames_received == 3
    assert result.expected_frames_hex == result.actual_frames_hex
    assert transport.closed is True


def test_hmi_without_endpoint_is_skipped() -> None:
    result = run_lcp_hmi_echo(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM10"),
        StationConfig(lcp_port="COM10", hmi_endpoint=None),
    )

    assert result.result == TestStatus.SKIPPED
