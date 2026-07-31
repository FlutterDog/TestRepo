"""Aggregate full-test models."""

from __future__ import annotations

from datetime import datetime
from enum import Enum

from pydantic import BaseModel, Field

from lorentz_test.models.tests import TestStatus


class FullTestStatus(str, Enum):
    PASS = "PASS"
    FAIL = "FAIL"
    FIXTURE_ERROR = "FIXTURE_ERROR"
    INCOMPLETE = "INCOMPLETE"


class FullTestStageResult(BaseModel):
    key: str
    title: str
    raw_result: TestStatus
    effective_result: FullTestStatus
    started_at: datetime
    duration_ms: int
    test_id: str | None = None
    report_file: str | None = None
    detail: str
    error: str | None = None
    blocked_by: str | None = None


class HardwareVerificationPoint(BaseModel):
    key: str
    title: str
    group: str
    result: TestStatus
    evaluated: bool
    configured: bool
    source_stage: str
    detail: str
    report_file: str | None = None


class FullTestSummary(BaseModel):
    total_points: int
    evaluated_points: int
    passed_points: int
    failed_points: int
    fixture_error_points: int
    skipped_points: int
    pending_points: list[str] = Field(default_factory=list)
    failed_point_names: list[str] = Field(default_factory=list)


class LcpFullTestResult(BaseModel):
    utility_version: str = "0.8.0"
    test_profile_version: str = "lcp2116-full-sequential-v1"
    test_id: str = "lcp_full_sequential"
    device_type: str = "LCP2116"
    result: FullTestStatus
    started_at: datetime
    duration_ms: int
    station_name: str
    serial_number: str
    operator: str
    port: str
    stages: list[FullTestStageResult] = Field(default_factory=list)
    hardware: list[HardwareVerificationPoint] = Field(default_factory=list)
    summary: FullTestSummary
    note: str = (
        "Backend последовательно выполняет безопасные штатные тесты. "
        "Flash A/B, watchdog reset и RTC retention запускаются отдельно."
    )
    error: str | None = None
    report_file: str | None = None
