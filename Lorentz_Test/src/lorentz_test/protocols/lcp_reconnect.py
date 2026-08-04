"""Windows-friendly LCP CDC identity and reconnect helpers."""

from __future__ import annotations

import dataclasses
import time
from collections.abc import Callable

from serial.tools import list_ports

from lorentz_test.protocols.lcp_usb import LcpUsbClient, ProtocolError, SerialTransportError


@dataclasses.dataclass(frozen=True)
class PortIdentity:
    vid: int | None
    pid: int | None
    serial_number: str | None


PortLister = Callable[[], object]
ClientFactory = Callable[..., LcpUsbClient]


def capture_port_identity(port: str) -> PortIdentity:
    for item in list_ports.comports():
        if item.device.casefold() == port.casefold():
            return PortIdentity(item.vid, item.pid, item.serial_number)
    return PortIdentity(None, None, None)


def candidate_ports(preferred: str, identity: PortIdentity) -> list[str]:
    result: list[str] = []
    for item in list_ports.comports():
        same_name = item.device.casefold() == preferred.casefold()
        same_identity = (
            identity.vid is not None
            and identity.pid is not None
            and item.vid == identity.vid
            and item.pid == identity.pid
            and (
                identity.serial_number is None
                or item.serial_number == identity.serial_number
            )
        )
        if same_name or same_identity:
            if item.device not in result:
                result.append(item.device)
    return result


def wait_for_lcp_reconnect(
    preferred: str,
    identity: PortIdentity,
    *,
    baudrate: int,
    serial_timeout: float,
    total_timeout: float = 35.0,
    retry_delay: float = 0.25,
    client_factory: ClientFactory = LcpUsbClient,
    sleep: Callable[[float], None] = time.sleep,
) -> str:
    """Return the COM name after a reboot and successful HELLO."""
    deadline = time.monotonic() + total_timeout
    last_error: Exception | None = None

    while time.monotonic() < deadline:
        candidates = candidate_ports(preferred, identity)
        if not candidates and preferred:
            candidates = [preferred]

        for candidate in candidates:
            try:
                with client_factory(
                    candidate,
                    baudrate=baudrate,
                    timeout=serial_timeout,
                    open_retry_seconds=min(3.0, max(0.5, deadline - time.monotonic())),
                    hello_retry_seconds=min(5.0, max(1.0, deadline - time.monotonic())),
                ) as client:
                    client.hello()
                return candidate
            except (ProtocolError, SerialTransportError, OSError) as exc:
                last_error = exc

        sleep(retry_delay)

    raise ProtocolError(
        f"LCP did not reconnect within {total_timeout:.0f} seconds: "
        f"{last_error or 'no matching CDC port'}"
    ) from last_error
