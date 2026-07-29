"""Serial port discovery models."""

from __future__ import annotations

from pydantic import BaseModel


class SerialPortInfo(BaseModel):
    device: str
    description: str
    hwid: str
    vid: int | None = None
    pid: int | None = None
    serial_number: str | None = None
    manufacturer: str | None = None
    product: str | None = None
    interface: str | None = None
    location: str | None = None
    lcp_candidate: bool = False
