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
    utility_version: str = "0.3.0"
    test_profile_version: str = "lcp2116-usb-identity-v1"
    test_id: str = "lcp_usb_identity"
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
    expected_firmware_version: str = "1.02.0"
    firmware_name: str | None = None
    firmware_version: str | None = None
    firmware_stage: str | None = None
    firmware_target: str | None = None
    firmware_version_verified: bool = False
    firmware_note: str = "Версия firmware читается через диагностическую консоль USB."
    firmware_raw_output: str | None = None
    error: str | None = None
    report_file: str | None = None


class DiagnosticCommandResult(BaseModel):
    command_id: str
    command: str
    title: str
    status: TestStatus
    duration_ms: int
    parsed_values: dict[str, str] = Field(default_factory=dict)
    raw_output: str | None = None
    error: str | None = None


class LcpDiagnosticsResult(BaseModel):
    utility_version: str = "0.3.0"
    test_profile_version: str = "lcp2116-diagnostic-snapshot-v1"
    test_id: str = "lcp_diagnostic_snapshot"
    device_type: str = "LCP2116"
    result: TestStatus
    started_at: datetime
    duration_ms: int
    station_name: str
    serial_number: str
    operator: str
    port: str
    evaluation_mode: str = "capture_only"
    commands: list[DiagnosticCommandResult] = Field(default_factory=list)
    note: str = (
        "PASS означает успешное получение ответа команды. Семантические аппаратные "
        "критерии будут применены после подтверждения реальных диагностических данных."
    )
    error: str | None = None
    report_file: str | None = None
