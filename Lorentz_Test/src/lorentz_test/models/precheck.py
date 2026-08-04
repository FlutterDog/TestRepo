"""Models for the non-destructive station readiness check."""

from __future__ import annotations

from datetime import datetime
from enum import Enum

from pydantic import BaseModel, Field

from lorentz_test import __version__


class PrecheckStatus(str, Enum):
    READY = "READY"
    NOT_READY = "NOT_READY"


class PrecheckCheckStatus(str, Enum):
    PASS = "PASS"
    WARNING = "WARNING"
    FAIL = "FAIL"
    SKIPPED = "SKIPPED"


class PrecheckCheck(BaseModel):
    key: str
    title: str
    status: PrecheckCheckStatus
    blocking: bool
    detail: str


class LcpPrecheckResult(BaseModel):
    utility_version: str = __version__
    test_profile_version: str = "lcp2116-station-precheck-v1"
    test_id: str = "lcp_station_precheck"
    device_type: str = "LCP2116"
    result: PrecheckStatus
    ready: bool
    started_at: datetime
    duration_ms: int
    station_name: str
    serial_number: str
    operator: str
    port: str
    firmware_version: str | None = None
    checks: list[PrecheckCheck] = Field(default_factory=list)
    blocking_failures: list[str] = Field(default_factory=list)
    warnings: list[str] = Field(default_factory=list)
    error: str | None = None
