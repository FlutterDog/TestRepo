import struct

import pytest

from lorentz_test.protocols.lcp_usb import (
    COMMAND_HELLO,
    FRAME_HEADER_SIZE,
    FRAME_MAGIC,
    HelloInfo,
    ProtocolError,
    build_frame,
    crc32,
    parse_frame,
)


def test_request_frame_matches_reference_layout() -> None:
    frame = build_frame(COMMAND_HELLO, 7)
    assert len(frame) == FRAME_HEADER_SIZE
    assert frame[:4] == FRAME_MAGIC
    assert struct.unpack_from("<I", frame, 8)[0] == 7
    assert struct.unpack_from("<I", frame, 16)[0] == crc32(b"")
    assert struct.unpack_from("<I", frame, 20)[0] == crc32(frame[:20])


def test_hello_payload_layout() -> None:
    payload = struct.pack("<HHHHIHH", 1, 1, 104, 160, 0x0F, 1, 24)
    hello = HelloInfo.from_payload(payload)
    assert hello.protocol_version == 1
    assert hello.bundle_size == 104
    assert hello.header_size == 24


def test_parse_frame_rejects_corrupt_header_crc() -> None:
    frame = bytearray(build_frame(COMMAND_HELLO, 1))
    frame[20] ^= 0x01
    with pytest.raises(ProtocolError, match="header CRC"):
        parse_frame(bytes(frame), b"")
