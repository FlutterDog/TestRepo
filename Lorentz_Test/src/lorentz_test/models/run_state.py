"""Persistent state models for asynchronous hardware test runs."""

from __future__ import annotations

from datetime import datetime
from enum import Enum
from typing import Any

from pydantic import BaseModel, Field

from lorentz_test.models.full_test import FullTestStatus, LcpFullTestResult
from lorentz_test.models.tests import LcpHelloRequest, TestStatus


class RunLifecycle(str, Enum):
    WAITING = "WAITING"
    RUNNING = "RUNNING"
    COMPLETE = "COMPLETE"
    ERROR = "ERROR"


class RunStageProgress(BaseModel):
    key: str
    title: str
    status: TestStatus = TestStatus.WAITING
    effective_result: FullTestStatus | None = None
    started_at: datetime | None = None
    completed_at: datetime | None = None
    duration_ms: int | None = None
    detail: str | None = None
    report_file: str | None = None
    blocked_by: str | None = None


class FullTestRunState(BaseModel):
    schema_version: int = 1
    run_id: str
    lifecycle: RunLifecycle
    created_at: datetime
    updated_at: datetime
    started_at: datetime | None = None
    completed_at: datetime | None = None
    request: LcpHelloRequest
    station_snapshot: dict[str, Any] = Field(default_factory=dict)
    current_stage_key: str | None = None
    current_stage_title: str | None = None
    current_stage_index: int = 0
    total_stages: int = 6
    stages: list[RunStageProgress] = Field(default_factory=list)
    result: LcpFullTestResult | None = None
    report_file: str | None = None
    error: str | None = None
