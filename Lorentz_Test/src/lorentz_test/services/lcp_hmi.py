"""Active HMI RS-485 echo test for LCP Basic Firmware 1.02.0."""

from __future__ import annotations

import time
from collections.abc import Callable
from datetime import datetime, timezone

import serial

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpHelloRequest, LcpHmiResult, TestStatus

SerialFactory = Callable[..., object]


def _serial_url(endpoint: str) -> str:
    if endpoint.casefold().startswith("tcp://"):
        return "socket://" + endpoint[6:]
    return endpoint


def _open_serial(endpoint: str, **kwargs: object) -> object:
    return serial.serial_for_url(_serial_url(endpoint), **kwargs)


def _read_exact(transport: object, length: int, timeout: float) -> bytes:
    output = bytearray()
    deadline = time.monotonic() + timeout
    while len(output) < length and time.monotonic() < deadline:
        chunk = transport.read(length - len(output))
        if chunk:
            output.extend(chunk)
    return bytes(output)


def run_lcp_hmi_echo(
    request: LcpHelloRequest,
    station: StationConfig,
    *,
    serial_factory: SerialFactory = _open_serial,
    sleep: Callable[[float], None] = time.sleep,
) -> LcpHmiResult:
    """Send three bounded frames and require exact delayed echo from the HMI port."""
    port = request.port or station.lcp_port or ""
    endpoint = station.hmi_endpoint
    started_at = datetime.now(timezone.utc)
    started = time.monotonic()
    if not endpoint:
        return LcpHmiResult(
            result=TestStatus.SKIPPED,
            started_at=started_at,
            duration_ms=0,
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port=port,
            endpoint=None,
            detail="HMI endpoint стенда не настроен",
        )

    frames = [
        b"LCP-HMI-ECHO-01",
        bytes((0x55, 0xAA, 0x00, 0xFF, 0x13, 0x37, 0x5A, 0xA5)),
        bytes((index * 17 + 3) & 0xFF for index in range(32)),
    ]
    expected_hex = [frame.hex(" ").upper() for frame in frames]
    actual: list[bytes] = []
    transport: object | None = None

    try:
        transport = serial_factory(
            endpoint,
            baudrate=9600,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.05,
            write_timeout=1.0,
        )
        transport.reset_input_buffer()
        transport.reset_output_buffer()

        for frame in frames:
            transport.write(frame)
            transport.flush()
            response = _read_exact(transport, len(frame), 2.0)
            actual.append(response)
            sleep(0.05)

        received_hex = [frame.hex(" ").upper() for frame in actual]
        passed = len(actual) == len(frames) and all(
            response == expected for response, expected in zip(actual, frames)
        )
        return LcpHmiResult(
            result=TestStatus.PASS if passed else TestStatus.FAIL,
            started_at=started_at,
            duration_ms=round((time.monotonic() - started) * 1000),
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port=port,
            endpoint=endpoint,
            frames_sent=len(frames),
            frames_received=sum(1 for frame in actual if frame),
            expected_frames_hex=expected_hex,
            actual_frames_hex=received_hex,
            detail=(
                "three HMI echo frames matched exactly"
                if passed
                else "HMI endpoint opened and frames were sent, but exact echo was not received"
            ),
        )
    except (serial.SerialException, OSError) as exc:
        return LcpHmiResult(
            result=TestStatus.FIXTURE_ERROR,
            started_at=started_at,
            duration_ms=round((time.monotonic() - started) * 1000),
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port=port,
            endpoint=endpoint,
            frames_sent=len(actual),
            frames_received=sum(1 for frame in actual if frame),
            expected_frames_hex=expected_hex,
            actual_frames_hex=[frame.hex(" ").upper() for frame in actual],
            detail=f"Python could not use HMI fixture endpoint: {exc}",
            error=f"{type(exc).__name__}: {exc}",
        )
    except Exception as exc:
        return LcpHmiResult(
            result=TestStatus.FAIL,
            started_at=started_at,
            duration_ms=round((time.monotonic() - started) * 1000),
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port=port,
            endpoint=endpoint,
            frames_sent=len(actual),
            frames_received=sum(1 for frame in actual if frame),
            expected_frames_hex=expected_hex,
            actual_frames_hex=[frame.hex(" ").upper() for frame in actual],
            detail=f"unexpected HMI test error: {type(exc).__name__}: {exc}",
            error=f"{type(exc).__name__}: {exc}",
        )
    finally:
        if transport is not None:
            try:
                transport.close()
            except Exception:
                pass
