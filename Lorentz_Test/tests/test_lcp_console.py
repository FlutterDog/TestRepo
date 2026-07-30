from lorentz_test.protocols.lcp_console import (
    LcpDiagnosticConsole,
    parse_key_value_output,
)


class FakeSerial:
    def __init__(self, response: bytes) -> None:
        self.timeout = 2.0
        self.response = response
        self.written = b""
        self.input_reset = False

    def reset_input_buffer(self) -> None:
        self.input_reset = True

    def write(self, data: bytes) -> int:
        self.written += data
        return len(data)

    def flush(self) -> None:
        pass

    def read(self, _: int = 1) -> bytes:
        if not self.response:
            return b""
        response, self.response = self.response, b""
        return response


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


def test_read_firmware_identity_uses_version_command() -> None:
    serial = FakeSerial(
        b"name = LCP Basic Diagnostic Firmware\r\n"
        b"version = 1.02.0\r\n"
        b"stage = Release 1.02.0\r\n"
        b"target = ATSAM3X8E\r\n"
    )
    console = LcpDiagnosticConsole(
        serial,
        command_timeout_seconds=0.1,
        quiet_seconds=0.0,
        session_settle_seconds=0.0,
    )

    identity = console.read_firmware_identity()

    assert serial.input_reset is True
    assert serial.written == b"version\r\n"
    assert identity.version == "1.02.0"
    assert identity.stage == "Release 1.02.0"
    assert identity.target == "ATSAM3X8E"
