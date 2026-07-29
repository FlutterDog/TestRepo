"""Test request and result models."""

from __future__ import annotations

from datetime import datetime
from enum import Enum

from pydantic import BaseModel, ConfigDict, Field


class TestStatus(str, Enum):
    WAITING = "WAITING"
    RUNNING = "RUNNING"
    PASS = "PASS"
    FAIL = "FAIL"
    SKIPPED = "SKIPPED"


class LcpHelloRequest(BaseModel):
    model_config = ConfigDict(extra="forbid", str_strip_whitespace=True)

    serial_number: str = Field(min_length=1, max_length=80)
    operator: str = Field(min_length=1, max_length=80)
    port: str | None = None


class CheckResult(BaseModel):
    name: str
    expected: str
    actual: str
    status: TestStatus


class HelloPayload(BaseModel):
    protocol_version: int
    schema_version: int
    bundle_size: int
    max_payload: int
    capabilities: int
    storage_version: int
    header_size: int


class LcpHelloResult(BaseModel):
    test_id: str = "lcp_usb_hello"
    device_type: str = "LCP2116"
    result: TestStatus
    started_at: datetime
    duration_ms: int
    station_name: str
    serial_number: str
    operator: str
    port: str
    hello: HelloPayload | None = None
    checks: list[CheckResult] = Field(default_factory=list)
    firmware_version: str | None = None
    firmware_version_verified: bool = False
    firmware_note: str = "HELLO does not return the human-readable firmware version"
    error: str | None = None
