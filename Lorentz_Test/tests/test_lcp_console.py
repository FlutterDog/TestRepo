from lorentz_test.protocols.lcp_console import (
    LcpDiagnosticConsole,
    parse_key_value_output,
)


class FakeSerial:
    def __init__(self, responses: bytes | list[bytes]) -> None:
        self.timeout = 2.0
        self.responses = [responses] if isinstance(responses, bytes) else list(responses)
        self.written = b""
        self.input_reset_count = 0
        self.output_reset_count = 0
        self.is_open = True

    def reset_input_buffer(self) -> None:
        self.input_reset_count += 1

    def reset_output_buffer(self) -> None:
        self.output_reset_count += 1

    def write(self, data: bytes) -> int:
        self.written += data
        return len(data)

    def flush(self) -> None:
        pass

    def close(self) -> None:
        self.is_open = False

    def read(self, _: int = 1) -> bytes:
        if not self.responses:
            return b""
        response = self.responses[0]
        if response:
            self.responses[0] = b""
            return response
        self.responses.pop(0)
        return b""


def test_parse_key_value_output_ignores_banners_and_order() -> None:
    values = parse_key_value_output(
        "=== FIRMWARE VERSION ===\r\n"
        "target = ATSAM3X8E\r\n"
        "name = LCP Basic Diagnostic Firmware\r\n"
        "stage = Release 1.02.0\r\n"
        "version = 1.02.0\r\n"
    )
    assert values == {
        "target": "ATSAM3X8E",
        "name": "LCP Basic Diagnostic Firmware",
        "stage": "Release 1.02.0",
        "version": "1.02.0",
    }


def _version_response() -> bytes:
    return (
        b"name = LCP Basic Diagnostic Firmware\r\n"
        b"version = 1.02.0\r\n"
        b"stage = Release 1.02.0\r\n"
        b"target = ATSAM3X8E\r\n"
    )


def test_read_firmware_identity_uses_version_command() -> None:
    serial = FakeSerial(_version_response())
    console = LcpDiagnosticConsole(
        serial,
        command_timeout_seconds=0.02,
        quiet_seconds=0.0,
        session_settle_seconds=0.0,
    )

    identity = console.read_firmware_identity()

    assert serial.input_reset_count == 1
    assert serial.written == b"version\r\n"
    assert identity.version == "1.02.0"
    assert identity.stage == "Release 1.02.0"
    assert identity.target == "ATSAM3X8E"


def test_version_command_retries_when_first_packet_is_swallowed() -> None:
    serial = FakeSerial([b"", _version_response()])
    console = LcpDiagnosticConsole(
        serial,
        command_timeout_seconds=0.01,
        quiet_seconds=0.0,
        session_settle_seconds=0.0,
        command_attempts=3,
        retry_delay_seconds=0.0,
    )

    identity = console.read_firmware_identity()

    assert identity.version == "1.02.0"
    assert serial.written == b"version\r\nversion\r\n"
    assert serial.input_reset_count == 2