"""Minimal deterministic Modbus RTU slave used by the LCP production fixture."""

from __future__ import annotations

import dataclasses
import threading
import time
from collections.abc import Callable

import serial

RegisterProvider = Callable[[int, int], list[int] | None]
WriteHandler = Callable[[int, int], bool]


def modbus_crc16(data: bytes) -> int:
    """Return standard Modbus RTU CRC16."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc & 0xFFFF


def append_crc(payload: bytes) -> bytes:
    crc = modbus_crc16(payload)
    return payload + bytes((crc & 0xFF, (crc >> 8) & 0xFF))


def frame_crc_valid(frame: bytes) -> bool:
    if len(frame) < 4:
        return False
    expected = frame[-2] | (frame[-1] << 8)
    return modbus_crc16(frame[:-2]) == expected


@dataclasses.dataclass
class RtuRequest:
    slave: int
    function: int
    address: int
    count_or_value: int


@dataclasses.dataclass
class RtuSlaveStats:
    requests_received: int = 0
    responses_sent: int = 0
    crc_errors: int = 0
    protocol_errors: int = 0
    io_errors: int = 0
    last_error: str | None = None
    requests: list[RtuRequest] = dataclasses.field(default_factory=list)

    def record(self, request: RtuRequest) -> None:
        self.requests_received += 1
        self.requests.append(request)
        if len(self.requests) > 64:
            del self.requests[:-64]


def build_response(
    frame: bytes,
    *,
    slave_address: int,
    register_provider: RegisterProvider,
    write_handler: WriteHandler | None = None,
) -> tuple[bytes | None, RtuRequest | None, str | None]:
    """Handle one fixed-size FC03/FC06 request and return a response frame."""
    if len(frame) != 8:
        return None, None, "request length is not 8 bytes"
    if not frame_crc_valid(frame):
        return None, None, "CRC mismatch"

    slave = frame[0]
    function = frame[1]
    address = (frame[2] << 8) | frame[3]
    count_or_value = (frame[4] << 8) | frame[5]
    request = RtuRequest(slave, function, address, count_or_value)

    if slave != slave_address:
        return None, request, None

    if function == 0x03:
        count = count_or_value
        if count < 1 or count > 125:
            return append_crc(bytes((slave, 0x83, 0x03))), request, "invalid register count"
        values = register_provider(address, count)
        if values is None or len(values) != count:
            return append_crc(bytes((slave, 0x83, 0x02))), request, "illegal data address"
        payload = bytearray((slave, function, count * 2))
        for value in values:
            value &= 0xFFFF
            payload.extend(((value >> 8) & 0xFF, value & 0xFF))
        return append_crc(bytes(payload)), request, None

    if function == 0x06:
        accepted = write_handler(address, count_or_value) if write_handler else False
        if not accepted:
            return append_crc(bytes((slave, 0x86, 0x02))), request, "write rejected"
        return frame, request, None

    return append_crc(bytes((slave, function | 0x80, 0x01))), request, "unsupported function"


class RtuSlaveServer:
    """Background serial RTU slave. The port is acquired synchronously in start()."""

    def __init__(
        self,
        *,
        port: str,
        baudrate: int,
        parity: str,
        stopbits: float,
        slave_address: int,
        register_provider: RegisterProvider,
        write_handler: WriteHandler | None = None,
        serial_opener: Callable[..., serial.Serial] = serial.Serial,
    ) -> None:
        self.port = port
        self.baudrate = baudrate
        self.parity = parity
        self.stopbits = stopbits
        self.slave_address = slave_address
        self.register_provider = register_provider
        self.write_handler = write_handler
        self.serial_opener = serial_opener
        self.stats = RtuSlaveStats()
        self._serial: serial.Serial | None = None
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        if self._serial is not None:
            raise RuntimeError(f"{self.port} is already open")
        self._serial = self.serial_opener(
            port=self.port,
            baudrate=self.baudrate,
            bytesize=serial.EIGHTBITS,
            parity=self.parity,
            stopbits=self.stopbits,
            timeout=0.05,
            write_timeout=0.5,
        )
        self._serial.reset_input_buffer()
        self._serial.reset_output_buffer()
        self._stop.clear()
        self._thread = threading.Thread(
            target=self._run,
            name=f"modbus-slave-{self.port}",
            daemon=True,
        )
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=1.5)
        if self._serial is not None:
            try:
                self._serial.close()
            finally:
                self._serial = None
        self._thread = None

    def _run(self) -> None:
        assert self._serial is not None
        buffer = bytearray()
        while not self._stop.is_set():
            try:
                chunk = self._serial.read(max(1, self._serial.in_waiting))
                if chunk:
                    buffer.extend(chunk)
                else:
                    time.sleep(0.002)
                self._consume(buffer)
            except (serial.SerialException, OSError) as exc:
                self.stats.io_errors += 1
                self.stats.last_error = f"{type(exc).__name__}: {exc}"
                return

    def _consume(self, buffer: bytearray) -> None:
        assert self._serial is not None
        while len(buffer) >= 8:
            if buffer[0] != self.slave_address or buffer[1] not in (0x03, 0x06):
                del buffer[0]
                continue
            frame = bytes(buffer[:8])
            if not frame_crc_valid(frame):
                self.stats.crc_errors += 1
                del buffer[0]
                continue
            del buffer[:8]
            response, request, error = build_response(
                frame,
                slave_address=self.slave_address,
                register_provider=self.register_provider,
                write_handler=self.write_handler,
            )
            if request is not None:
                self.stats.record(request)
            if error is not None:
                self.stats.protocol_errors += 1
                self.stats.last_error = error
            if response is not None:
                self._serial.write(response)
                self._serial.flush()
                self.stats.responses_sent += 1

    def __enter__(self) -> "RtuSlaveServer":
        self.start()
        return self

    def __exit__(self, *_: object) -> None:
        self.stop()
