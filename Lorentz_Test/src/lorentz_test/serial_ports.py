"""Cross-platform serial-port enumeration with Windows COM metadata."""

from __future__ import annotations

import re
from typing import Any

from lorentz_test.models.serial import SerialPortInfo

_LCP_HINTS = ("lorentz", "lcp", "arduino due", "atmel", "sam3x")
_COM_NUMBER = re.compile(r"^COM(\d+)$", re.IGNORECASE)


def _port_sort_key(device: str) -> tuple[int, int | str]:
    match = _COM_NUMBER.match(device)
    if match:
        return 0, int(match.group(1))
    return 1, device.casefold()


def _to_info(item: Any) -> SerialPortInfo:
    searchable = " ".join(
        str(value or "")
        for value in (
            getattr(item, "device", None),
            getattr(item, "description", None),
            getattr(item, "hwid", None),
            getattr(item, "manufacturer", None),
            getattr(item, "product", None),
            getattr(item, "interface", None),
        )
    ).casefold()
    return SerialPortInfo(
        device=str(item.device),
        description=str(item.description or ""),
        hwid=str(item.hwid or ""),
        vid=item.vid,
        pid=item.pid,
        serial_number=item.serial_number,
        manufacturer=item.manufacturer,
        product=item.product,
        interface=item.interface,
        location=item.location,
        lcp_candidate=any(hint in searchable for hint in _LCP_HINTS),
    )


def list_serial_ports() -> list[SerialPortInfo]:
    try:
        from serial.tools import list_ports
    except ImportError as exc:  # pragma: no cover - installation problem
        raise RuntimeError("pyserial is not installed") from exc

    ports = [_to_info(item) for item in list_ports.comports()]
    return sorted(ports, key=lambda item: _port_sort_key(item.device))
