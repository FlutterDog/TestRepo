import struct

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import ConfirmedTestRequest, TestStatus
from lorentz_test.protocols.lcp_usb import (
    COMMAND_GET_CONFIG,
    COMMAND_GET_STATUS,
    COMMAND_PUT_CONFIG,
    COMMAND_VALIDATE_CONFIG,
    Frame,
    STATUS_ACCEPTED,
    STATUS_OK,
    STATUS_REBOOT_REQUIRED,
    crc32,
)
from lorentz_test.services.lcp_flash_ab import (
    _read_bundle,
    _test_bundle,
    _write_bundle,
    run_lcp_flash_ab,
)


class FakeClient:
    def __init__(self) -> None:
        self.bundle = bytearray(104)
        struct.pack_into("<I", self.bundle, 12, 9600)
        checksum = crc32(bytes(self.bundle))
        self.config_payload = (
            struct.pack("<HHIII", 1, 104, 4, 8, checksum)
            + bytes((1, 0, 0, 0))
            + bytes(self.bundle)
        )
        self.calls: list[int] = []

    def request(self, command: int, payload: bytes = b"", **_: object) -> Frame:
        self.calls.append(command)
        if command == COMMAND_GET_CONFIG:
            return Frame(command, STATUS_OK, 0, 1, self.config_payload)
        if command == COMMAND_VALIDATE_CONFIG:
            assert len(payload) == 112
            return Frame(command, STATUS_OK, 0, 2, b"")
        if command == COMMAND_PUT_CONFIG:
            return Frame(command, STATUS_ACCEPTED, 0, 3, b"")
        if command == COMMAND_GET_STATUS:
            status = bytearray(28)
            status[0] = 4
            status[1] = STATUS_REBOOT_REQUIRED
            status[3] = 0
            status[4] = 1
            status[24] = 0
            return Frame(command, STATUS_OK, 0, 4, bytes(status))
        raise AssertionError(command)


def test_flash_bundle_mutation_changes_only_s4_baud() -> None:
    original = bytearray(range(104))
    struct.pack_into("<I", original, 12, 9600)
    mutated, old_baud, new_baud = _test_bundle(bytes(original))

    assert old_baud == 9600
    assert new_baud == 19200
    assert struct.unpack_from("<I", mutated, 12)[0] == 19200
    assert mutated[:12] == bytes(original[:12])
    assert mutated[16:] == bytes(original[16:])


def test_flash_helpers_parse_config_and_wait_for_complete_writer() -> None:
    client = FakeClient()
    snapshot = _read_bundle(client)
    mutated, _, _ = _test_bundle(snapshot.bundle)
    writer = _write_bundle(client, mutated)

    assert snapshot.slot == 0
    assert snapshot.stored_crc32 == crc32(snapshot.bundle)
    assert writer.state == 4
    assert writer.result == STATUS_REBOOT_REQUIRED
    assert writer.target_slot == 1
    assert COMMAND_VALIDATE_CONFIG in client.calls
    assert COMMAND_PUT_CONFIG in client.calls
    assert COMMAND_GET_STATUS in client.calls


def test_flash_test_requires_explicit_confirmation() -> None:
    result = run_lcp_flash_ab(
        ConfirmedTestRequest(
            serial_number="LCP-1",
            operator="Operator",
            port="COM10",
            confirmation="wrong",
        ),
        StationConfig(lcp_port="COM10"),
    )
    assert result.result == TestStatus.SKIPPED
