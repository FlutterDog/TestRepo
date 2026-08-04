"""Persistent asynchronous test-run controller and exclusive hardware lock."""

from __future__ import annotations

import os
import threading
from contextlib import contextmanager
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable, Iterator, Protocol
from uuid import uuid4

from lorentz_test.models.full_test import FullTestStageResult, LcpFullTestResult
from lorentz_test.models.run_state import (
    FullTestRunState,
    RunLifecycle,
    RunStageProgress,
)
from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpHelloRequest, TestStatus
from lorentz_test.paths import data_dir
from lorentz_test.reporting.json_report import report_writer
from lorentz_test.services.lcp_full_test import (
    full_test_stage_definitions,
    run_lcp_full_test,
)


class FullReportWriter(Protocol):
    def save_lcp_full_test(self, result: LcpFullTestResult) -> object: ...


FullRunner = Callable[..., LcpFullTestResult]


class HardwareBusyError(RuntimeError):
    def __init__(self, owner: str | None) -> None:
        self.owner = owner or "unknown operation"
        super().__init__(f"hardware resources are busy: {self.owner}")


class HardwareOperationGuard:
    """One process-wide non-reentrant lock for all DUT and fixture resources."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._metadata_lock = threading.Lock()
        self._owner: str | None = None

    @property
    def owner(self) -> str | None:
        with self._metadata_lock:
            return self._owner

    def acquire(self, owner: str) -> None:
        if not self._lock.acquire(blocking=False):
            raise HardwareBusyError(self.owner)
        with self._metadata_lock:
            self._owner = owner

    def release(self, owner: str) -> None:
        with self._metadata_lock:
            if self._owner != owner:
                raise RuntimeError(
                    f"hardware lock owner mismatch: expected {self._owner!r}, got {owner!r}"
                )
            self._owner = None
        self._lock.release()

    @contextmanager
    def hold(self, owner: str) -> Iterator[None]:
        self.acquire(owner)
        try:
            yield
        finally:
            self.release(owner)


class FullTestRunController:
    """Run one full verification in the background and persist live progress."""

    def __init__(
        self,
        *,
        state_file: Path | None = None,
        guard: HardwareOperationGuard | None = None,
        runner: FullRunner = run_lcp_full_test,
        writer: FullReportWriter = report_writer,
    ) -> None:
        self.state_file = state_file or (data_dir() / "full_test_run.json")
        self.guard = guard or HardwareOperationGuard()
        self.runner = runner
        self.writer = writer
        self._state_lock = threading.Lock()
        self._state: FullTestRunState | None = self._load_state()
        self._recover_interrupted_state()

    def _load_state(self) -> FullTestRunState | None:
        if not self.state_file.exists():
            return None
        try:
            return FullTestRunState.model_validate_json(
                self.state_file.read_text(encoding="utf-8")
            )
        except (OSError, ValueError):
            return None

    def _recover_interrupted_state(self) -> None:
        if self._state is None or self._state.lifecycle not in {
            RunLifecycle.WAITING,
            RunLifecycle.RUNNING,
        }:
            return
        now = datetime.now(timezone.utc)
        interrupted_lifecycle = self._state.lifecycle.value
        self._state.lifecycle = RunLifecycle.ERROR
        self._state.updated_at = now
        self._state.completed_at = now
        self._state.error = (
            f"Backend был остановлен при состоянии {interrupted_lifecycle}. "
            "Результат этого прогона недействителен; запустите проверку заново."
        )
        for stage in self._state.stages:
            if stage.status == TestStatus.RUNNING:
                stage.status = TestStatus.FIXTURE_ERROR
                stage.effective_result = None
                stage.completed_at = now
                stage.detail = "Backend process interrupted during this stage"
        self._state.current_stage_key = None
        self._state.current_stage_title = None
        self._persist_locked()

    def _persist_locked(self) -> None:
        if self._state is None:
            return
        self.state_file.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.state_file.with_suffix(self.state_file.suffix + ".tmp")
        try:
            temporary.write_text(
                self._state.model_dump_json(indent=2) + "\n",
                encoding="utf-8",
            )
            os.replace(temporary, self.state_file)
        except OSError as exc:
            temporary.unlink(missing_ok=True)
            raise RuntimeError(f"cannot persist full-test run state: {exc}") from exc

    def current(self) -> FullTestRunState | None:
        with self._state_lock:
            return self._state.model_copy(deep=True) if self._state else None

    def get(self, run_id: str) -> FullTestRunState | None:
        state = self.current()
        if state is None or state.run_id != run_id:
            return None
        return state

    def start(
        self,
        request: LcpHelloRequest,
        station: StationConfig,
    ) -> FullTestRunState:
        run_id = uuid4().hex
        owner = f"full-test:{run_id}"
        self.guard.acquire(owner)
        now = datetime.now(timezone.utc)
        state = FullTestRunState(
            run_id=run_id,
            lifecycle=RunLifecycle.WAITING,
            created_at=now,
            updated_at=now,
            request=request.model_copy(deep=True),
            station_snapshot=station.model_dump(mode="json"),
            total_stages=len(full_test_stage_definitions()),
            stages=[
                RunStageProgress(key=key, title=title)
                for key, title in full_test_stage_definitions()
            ],
        )

        try:
            with self._state_lock:
                self._state = state
                self._persist_locked()
            thread = threading.Thread(
                target=self._worker,
                args=(owner, run_id, request.model_copy(deep=True), station.model_copy(deep=True)),
                name=f"lorentz-full-test-{run_id[:8]}",
                daemon=True,
            )
            thread.start()
        except Exception as exc:
            failed = datetime.now(timezone.utc)
            with self._state_lock:
                if self._state is not None and self._state.run_id == run_id:
                    self._state.lifecycle = RunLifecycle.ERROR
                    self._state.updated_at = failed
                    self._state.completed_at = failed
                    self._state.error = f"{type(exc).__name__}: {exc}"
                    try:
                        self._persist_locked()
                    except RuntimeError:
                        pass
            self.guard.release(owner)
            raise
        return self.current() or state

    def _worker(
        self,
        owner: str,
        run_id: str,
        request: LcpHelloRequest,
        station: StationConfig,
    ) -> None:
        try:
            now = datetime.now(timezone.utc)
            with self._state_lock:
                if self._state is None or self._state.run_id != run_id:
                    return
                self._state.lifecycle = RunLifecycle.RUNNING
                self._state.started_at = now
                self._state.updated_at = now
                self._persist_locked()

            def progress(
                event: str,
                index: int,
                key: str,
                title: str,
                stage: FullTestStageResult | None,
            ) -> None:
                self._update_progress(run_id, event, index, key, title, stage)

            result = self.runner(
                request,
                station,
                run_id=run_id,
                progress_callback=progress,
            )
            self.writer.save_lcp_full_test(result)
            completed = datetime.now(timezone.utc)
            with self._state_lock:
                if self._state is None or self._state.run_id != run_id:
                    return
                self._state.lifecycle = RunLifecycle.COMPLETE
                self._state.updated_at = completed
                self._state.completed_at = completed
                self._state.current_stage_key = None
                self._state.current_stage_title = None
                self._state.current_stage_index = self._state.total_stages
                self._state.result = result
                self._state.report_file = result.report_file
                self._state.error = None
                self._persist_locked()
        except Exception as exc:
            completed = datetime.now(timezone.utc)
            with self._state_lock:
                if self._state is not None and self._state.run_id == run_id:
                    self._state.lifecycle = RunLifecycle.ERROR
                    self._state.updated_at = completed
                    self._state.completed_at = completed
                    self._state.current_stage_key = None
                    self._state.current_stage_title = None
                    self._state.error = f"{type(exc).__name__}: {exc}"
                    try:
                        self._persist_locked()
                    except RuntimeError:
                        pass
        finally:
            self.guard.release(owner)

    def _update_progress(
        self,
        run_id: str,
        event: str,
        index: int,
        key: str,
        title: str,
        stage: FullTestStageResult | None,
    ) -> None:
        now = datetime.now(timezone.utc)
        with self._state_lock:
            if self._state is None or self._state.run_id != run_id:
                return
            item = next((value for value in self._state.stages if value.key == key), None)
            if item is None:
                return
            self._state.updated_at = now
            self._state.current_stage_index = index
            if event == "started":
                self._state.current_stage_key = key
                self._state.current_stage_title = title
                item.status = TestStatus.RUNNING
                item.started_at = now
                item.detail = "Этап выполняется"
            elif event == "completed" and stage is not None:
                item.status = stage.raw_result
                item.effective_result = stage.effective_result
                item.started_at = stage.started_at
                item.completed_at = now
                item.duration_ms = stage.duration_ms
                item.detail = stage.detail
                item.report_file = stage.report_file
                item.blocked_by = stage.blocked_by
                self._state.current_stage_key = key
                self._state.current_stage_title = title
            self._persist_locked()


hardware_operation_guard = HardwareOperationGuard()
full_test_run_controller = FullTestRunController(guard=hardware_operation_guard)
