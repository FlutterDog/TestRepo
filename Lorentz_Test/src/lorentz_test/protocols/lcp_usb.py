"""LCP2116 binary USB configuration protocol client.

The framing and HELLO payload follow the verified LCP Basic Firmware v1.02.0
reference utility. HELLO identifies protocol capabilities; it does not carry the
human-readable firmware version string.
"""

from __future__ import annotations

import dataclasses
import struct
import time
import zlib
from collections.abc import Callable
from typing import Any, Protocol

PROTOCOL_VERSION = 1
FRAME_MAGIC = b"\x00LCP"
FRAME_HEADER_SIZE = 24
MAX_PAYLOAD_SIZE = 160

COMMAND_HELLO = 1
COMMAND_GET_CONFIG = 2
COMMAND_VALIDATE_CONFIG = 3
COMMAND_PUT_CONFIG = 4
COMMAND_GET_STATUS = 5
COMMAND_REBOOT = 6
COMMAND_EXIT = 7

STATUS_OK = 0
STATUS_ACCEPTED = 1
STATUS_BUSY = 2
STATUS_NO_CHANGE = 3
STATUS_INVALID_CRC = 4
STATUS_UNSUPPORTED_VERSION = 5
STATUS_UNSUPPORTED_COMMAND = 6
STATUS_INVALID_LENGTH = 7
STATUS_INVALID_SCHEMA = 8
STATUS_INVALID_CONFIG = 9
STATUS_FLASH_ERROR = 10
STATUS_VERIFY_ERROR = 11
STATUS_REBOOT_REQUIRED = 12
STATUS_INTERNAL_ERROR = 13

STATUS_TEXT = {
    STATUS_OK: "ok",
    STATUS_ACCEPTED: "accepted",
    STATUS_BUSY: "busy",
    STATUS_NO_CHANGE: "no change",
    STATUS_INVALID_CRC: "invalid CRC",
    STATUS_UNSUPPORTED_VERSION: "unsupported protocol version",
    STATUS_UNSUPPORTED_COMMAND: "unsupported command",
    STATUS_INVALID_LENGTH: "invalid length",
    STATUS_INVALID_SCHEMA: "invalid schema",
    STATUS_INVALID_CONFIG: "invalid configuration",
    STATUS_FLASH_ERROR: "Flash error",
    STATUS_VERIFY_ERROR: "verify error",
    STATUS_REBOOT_REQUIRED: "reboot required",
    STATUS_INTERNAL_ERROR: "internal error",
}


class ProtocolError(RuntimeError):
    """Malformed frame, timeout or device-side protocol error."""


class SerialTransportError(RuntimeError):
    """The operating system could not open or use the serial port."""


class SerialPort(Protocol):
    is_open: bool
    timeout: float | None

    def read(self, size: int = 1) -> bytes: ...
    def write(self, data: bytes) -> int: ...
    def flush(self) -> None: ...
    def close(self) -> None: ...
    def reset_input_buffer(self) -> None: ...
    def reset_output_buffer(self) -> None: ...


SerialFactory = Callable[[str, int, float], SerialPort]


@dataclasses.dataclass(frozen=True)
class Frame:
    command: int
    status: int
    flags: int
    request_id: int
    payload: bytes


@dataclasses.dataclass(frozen=True)
class HelloInfo:
    protocol_version: int
    schema_version: int
    bundle_size: int
    max_payload: int
    capabilities: int
    storage_version: int
    header_size: int

    @classmethod
    def from_payload(cls, payload: bytes) -> "HelloInfo":
        if len(payload) != 16:
            raise ProtocolError(f"invalid HELLO response length: {len(payload)}")
        return cls(*struct.unpack("<HHHHIHH", payload))

    def to_dict(self) -> dict[str, int]:
        return dataclasses.asdict(self)


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def build_frame(command: int, request_id: int, payload: bytes = b"") -> bytes:
    if not 0 <= command <= 0xFF:
        raise ValueError("command must fit one byte")
    if not 1 <= request_id <= 0xFFFFFFFF:
        raise ValueError("request_id must be in range 1..0xFFFFFFFF")
    if len(payload) > MAX_PAYLOAD_SIZE:
        raise ProtocolError("payload exceeds protocol maximum")

    prefix = struct.pack(
        "<4sBBBBIII",
        FRAME_MAGIC,
        PROTOCOL_VERSION,
        command,
        0,
        0,
        request_id,
        len(payload),
        crc32(payload),
    )
    return prefix + struct.pack("<I", crc32(prefix)) + payload


def parse_frame(header: bytes, payload: bytes) -> Frame:
    if len(header) != FRAME_HEADER_SIZE:
        raise ProtocolError(f"invalid frame header length: {len(header)}")
    (
        magic,
        version,
        command,
        status,
        flags,
        request_id,
        payload_length,
        payload_crc,
        header_crc,
    ) = struct.unpack("<4sBBBBIIII", header)

    if magic != FRAME_MAGIC:
        raise ProtocolError("invalid frame magic")
    if version != PROTOCOL_VERSION:
        raise ProtocolError(f"protocol version {version} is not supported")
    if crc32(header[:20]) != header_crc:
        raise ProtocolError("header CRC mismatch")
    if payload_length > MAX_PAYLOAD_SIZE:
        raise ProtocolError(f"response payload too large: {payload_length}")
    if len(payload) != payload_length:
        raise ProtocolError(f"payload length {len(payload)} != declared {payload_length}")
    if crc32(payload) != payload_crc:
        raise ProtocolError("payload CRC mismatch")
    return Frame(command, status, flags, request_id, payload)


def _open_pyserial(port: str, baudrate: int, timeout: float) -> SerialPort:
    try:
        import serial

        return serial.Serial(
            port=port,
            baudrate=baudrate,
            timeout=timeout,
            write_timeout=timeout,
        )
    except Exception as exc:
        raise SerialTransportError(str(exc)) from exc


class LcpUsbClient:
    """Robust Windows-friendly client for one LCP USB CDC port."""

    def __init__(
        self,
        port: str,
        *,
        baudrate: int = 115200,
        timeout: float = 2.0,
        open_retry_seconds: float = 8.0,
        open_settle_seconds: float = 0.50,
        hello_retry_seconds: float = 20.0,
        retry_delay_seconds: float = 0.25,
        serial_factory: SerialFactory | None = None,
    ) -> None:
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.open_retry_seconds = open_retry_seconds
        self.open_settle_seconds = open_settle_seconds
        self.hello_retry_seconds = hello_retry_seconds
        self.retry_delay_seconds = retry_delay_seconds
        self._serial_factory = serial_factory or _open_pyserial
        self.serial: SerialPort | None = None
        self._request_id = 1
        self._binary_session_active = False

    @property
    def transport(self) -> SerialPort:
        """Return the currently opened CDC transport without transferring ownership."""
        return self._require_serial()

    def connect(self) -> None:
        deadline = time.monotonic() + self.open_retry_seconds
        last_error: Exception | None = None
        while time.monotonic() < deadline:
            try:
                self.serial = self._serial_factory(self.port, self.baudrate, self.timeout)
                self._binary_session_active = False
                time.sleep(self.open_settle_seconds)
                self.serial.reset_input_buffer()
                self.serial.reset_output_buffer()
                return
            except (SerialTransportError, OSError) as exc:
                last_error = exc
                self._raw_close()
                time.sleep(self.retry_delay_seconds)
        raise SerialTransportError(
            f"cannot open {self.port}: {last_error or 'open timeout'}"
        ) from last_error

    def exit_binary_session(self) -> Frame | None:
        """Return the shared USB CDC port from the binary protocol to text console."""
        if self.serial is None or not self._binary_session_active:
            return None
        frame = self.request(COMMAND_EXIT, accepted={STATUS_OK}, timeout=0.5)
        self._binary_session_active = False
        return frame

    def close(self) -> None:
        if self.serial is None:
            return
        try:
            self.exit_binary_session()
        except Exception:
            pass
        self._raw_close()

    def _raw_close(self) -> None:
        serial_port, self.serial = self.serial, None
        self._binary_session_active = False
        if serial_port is None:
            return
        try:
            if serial_port.is_open:
                serial_port.close()
        except Exception:
            pass

    def __enter__(self) -> "LcpUsbClient":
        self.connect()
        return self

    def __exit__(self, exc_type: Any, exc: Any, traceback: Any) -> None:
        self.close()

    def _next_request_id(self) -> int:
        result = self._request_id
        self._request_id = (self._request_id + 1) & 0xFFFFFFFF
        if self._request_id == 0:
            self._request_id = 1
        return result

    def _require_serial(self) -> SerialPort:
        if self.serial is None:
            raise SerialTransportError("serial port is not connected")
        return self.serial

    def _read_exact(self, length: int, deadline: float) -> bytes:
        output = bytearray()
        serial_port = self._require_serial()
        while len(output) < length:
            if time.monotonic() >= deadline:
                raise ProtocolError(
                    f"timeout while reading {length} bytes; received {len(output)}"
                )
            chunk = serial_port.read(length - len(output))
            if chunk:
                output.extend(chunk)
        return bytes(output)

    def _find_magic(self, deadline: float) -> None:
        serial_port = self._require_serial()
        matched = 0
        while matched < len(FRAME_MAGIC):
            if time.monotonic() >= deadline:
                raise ProtocolError("timeout waiting for LCP USB frame")
            value = serial_port.read(1)
            if not value:
                continue
            byte = value[0]
            if byte == FRAME_MAGIC[matched]:
                matched += 1
            else:
                matched = 1 if byte == FRAME_MAGIC[0] else 0

    def _read_frame(self, timeout: float) -> Frame:
        deadline = time.monotonic() + timeout
        self._find_magic(deadline)
        rest = self._read_exact(FRAME_HEADER_SIZE - len(FRAME_MAGIC), deadline)
        header = FRAME_MAGIC + rest
        payload_length = struct.unpack_from("<I", header, 12)[0]
        if payload_length > MAX_PAYLOAD_SIZE:
            raise ProtocolError(f"response payload too large: {payload_length}")
        payload = self._read_exact(payload_length, deadline)
        return parse_frame(header, payload)

    def request(
        self,
        command: int,
        payload: bytes = b"",
        *,
        accepted: set[int] | None = None,
        timeout: float | None = None,
    ) -> Frame:
        serial_port = self._require_serial()
        request_id = self._next_request_id()
        if command != COMMAND_EXIT:
            self._binary_session_active = True
        try:
            serial_port.write(build_frame(command, request_id, payload))
            serial_port.flush()
        except Exception as exc:
            raise SerialTransportError(f"serial write failed: {exc}") from exc

        response_timeout = timeout if timeout is not None else self.timeout
        while True:
            frame = self._read_frame(response_timeout)
            if frame.request_id != request_id:
                continue
            if frame.command != command:
                raise ProtocolError(
                    f"response command {frame.command} != request {command}"
                )
            if accepted is not None and frame.status not in accepted:
                text = STATUS_TEXT.get(frame.status, f"status {frame.status}")
                raise ProtocolError(f"device returned {text}")
            if command == COMMAND_EXIT:
                self._binary_session_active = False
            return frame

    def hello_once(self) -> HelloInfo:
        if self.serial is None:
            self.connect()
        frame = self.request(COMMAND_HELLO, accepted={STATUS_OK})
        return HelloInfo.from_payload(frame.payload)

    def hello(self) -> HelloInfo:
        """Complete HELLO across transient usbser.sys open/read failures."""
        deadline = time.monotonic() + self.hello_retry_seconds
        last_error: Exception | None = None
        while time.monotonic() < deadline:
            try:
                return self.hello_once()
            except (ProtocolError, SerialTransportError, OSError) as exc:
                last_error = exc
                self._raw_close()
                time.sleep(self.retry_delay_seconds)
        raise ProtocolError(
            f"controller did not complete HELLO within {self.hello_retry_seconds:.0f} "
            f"seconds: {last_error or 'timeout'}"
        ) from last_error
