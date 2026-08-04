from __future__ import annotations

import time
from datetime import datetime, timezone
from pathlib import Path
from threading import Event

import pytest

from lorentz_test.models.full_test import (
    FullTestStageResult,
    FullTestStatus,
    FullTestSummary,
    LcpFullTestResult,
)
from lorentz_test.models.run_state import (
    FullTestRunState,
    RunLifecycle,
    RunStageProgress,
)
from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpHelloRequest, TestStatus as Status
from lorentz_test.services.lcp_full_test import full_test_stage_definitions
from lorentz_test.services.test_run_controller import (
    FullTestRunController,
    HardwareBusyError,
    HardwareOperationGuard,
)


class FakeWriter:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.calls = 0

    def save_lcp_full_test(self, result: LcpFullTestResult) -> Path:
        self.calls += 1
        result.report_file = str(self.path)
        return self.path


def request() -> LcpHelloRequest:
    return LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM10")


def station() -> StationConfig:
    return StationConfig(lcp_port="COM10")


def result_for(run_id: str, station_config: StationConfig) -> LcpFullTestResult:
    now = datetime.now(timezone.utc)
    stages = [
        FullTestStageResult(
            key=key,
            title=title,
            raw_result=Status.PASS,
            effective_result=FullTestStatus.PASS,
            started_at=now,
            duration_ms=1,
            detail="verified",
        )
        for key, title in full_test_stage_definitions()
    ]
    return LcpFullTestResult(
        run_id=run_id,
        result=FullTestStatus.PASS,
        started_at=now,
        completed_at=now,
        duration_ms=6,
        station_name=station_config.station_name,
        station_snapshot=station_config.model_dump(mode="json"),
        serial_number="LCP-1",
        operator="Operator",
        port="COM10",
        firmware_version="1.02.0",
        stages=stages,
        hardware=[],
        summary=FullTestSummary(
            total_points=0,
            evaluated_points=0,
            passed_points=0,
            failed_points=0,
            fixture_error_points=0,
            skipped_points=0,
        ),
    )


def wait_for_lifecycle(
    controller: FullTestRunController,
    expected: RunLifecycle,
    timeout: float = 2.0,
) -> FullTestRunState:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        state = controller.current()
        if state is not None and state.lifecycle == expected:
            return state
        time.sleep(0.01)
    raise AssertionError(f"run did not reach {expected}")


def wait_for_guard_free(guard: HardwareOperationGuard, timeout: float = 1.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if guard.owner is None:
            return
        time.sleep(0.005)
    raise AssertionError(f"hardware guard remained owned by {guard.owner}")


def test_controller_blocks_concurrent_hardware_and_persists_completion(tmp_path: Path) -> None:
    started = Event()
    release = Event()

    def runner(
        _request: LcpHelloRequest,
        station_config: StationConfig,
        *,
        run_id: str,
        progress_callback,
    ) -> LcpFullTestResult:
        now = datetime.now(timezone.utc)
        definitions = full_test_stage_definitions()
        for index, (key, title) in enumerate(definitions, start=1):
            progress_callback("started", index, key, title, None)
            if index == 1:
                started.set()
                assert release.wait(2.0)
            stage = FullTestStageResult(
                key=key,
                title=title,
                raw_result=Status.PASS,
                effective_result=FullTestStatus.PASS,
                started_at=now,
                duration_ms=1,
                detail="verified",
            )
            progress_callback("completed", index, key, title, stage)
        return result_for(run_id, station_config)

    guard = HardwareOperationGuard()
    writer = FakeWriter(tmp_path / "FULL_TEST.json")
    controller = FullTestRunController(
        state_file=tmp_path / "run.json",
        guard=guard,
        runner=runner,
        writer=writer,
    )

    initial = controller.start(request(), station())
    assert initial.lifecycle in {RunLifecycle.WAITING, RunLifecycle.RUNNING}
    assert started.wait(1.0)

    running = controller.current()
    assert running is not None
    assert running.lifecycle == RunLifecycle.RUNNING
    assert running.current_stage_key == "hello"
    assert running.stages[0].status == Status.RUNNING

    with pytest.raises(HardwareBusyError):
        controller.start(request(), station())
    with pytest.raises(HardwareBusyError):
        with guard.hold("manual-test"):
            pass

    release.set()
    completed = wait_for_lifecycle(controller, RunLifecycle.COMPLETE)
    wait_for_guard_free(guard)
    assert completed.result is not None
    assert completed.result.result == FullTestStatus.PASS
    assert completed.report_file == str(writer.path)
    assert writer.calls == 1
    assert all(stage.status == Status.PASS for stage in completed.stages)
    assert (tmp_path / "run.json").exists()

    restored = FullTestRunController(
        state_file=tmp_path / "run.json",
        guard=HardwareOperationGuard(),
        runner=runner,
        writer=writer,
    ).current()
    assert restored is not None
    assert restored.lifecycle == RunLifecycle.COMPLETE
    assert restored.run_id == completed.run_id
    assert restored.result is not None


@pytest.mark.parametrize("lifecycle", [RunLifecycle.WAITING, RunLifecycle.RUNNING])
def test_interrupted_session_is_recovered_as_error(
    tmp_path: Path,
    lifecycle: RunLifecycle,
) -> None:
    now = datetime.now(timezone.utc)
    stage_status = Status.RUNNING if lifecycle == RunLifecycle.RUNNING else Status.WAITING
    state = FullTestRunState(
        run_id="interrupted",
        lifecycle=lifecycle,
        created_at=now,
        updated_at=now,
        started_at=now if lifecycle == RunLifecycle.RUNNING else None,
        request=request(),
        station_snapshot=station().model_dump(mode="json"),
        current_stage_key="rs485" if lifecycle == RunLifecycle.RUNNING else None,
        current_stage_title="RS-485" if lifecycle == RunLifecycle.RUNNING else None,
        current_stage_index=3 if lifecycle == RunLifecycle.RUNNING else 0,
        stages=[
            RunStageProgress(
                key="rs485",
                title="RS-485",
                status=stage_status,
                started_at=now if lifecycle == RunLifecycle.RUNNING else None,
            )
        ],
    )
    path = tmp_path / "run.json"
    path.write_text(state.model_dump_json(indent=2), encoding="utf-8")

    recovered = FullTestRunController(state_file=path).current()

    assert recovered is not None
    assert recovered.lifecycle == RunLifecycle.ERROR
    assert recovered.completed_at is not None
    assert recovered.current_stage_key is None
    if lifecycle == RunLifecycle.RUNNING:
        assert recovered.stages[0].status == Status.FIXTURE_ERROR
    else:
        assert recovered.stages[0].status == Status.WAITING
    assert lifecycle.value in (recovered.error or "")


def test_runner_exception_marks_error_and_releases_hardware(tmp_path: Path) -> None:
    entered = Event()

    def failing_runner(*_args, **_kwargs) -> LcpFullTestResult:
        entered.set()
        raise RuntimeError("boom")

    guard = HardwareOperationGuard()
    controller = FullTestRunController(
        state_file=tmp_path / "run.json",
        guard=guard,
        runner=failing_runner,
        writer=FakeWriter(tmp_path / "unused.json"),
    )

    controller.start(request(), station())
    assert entered.wait(1.0)
    failed = wait_for_lifecycle(controller, RunLifecycle.ERROR)
    wait_for_guard_free(guard)

    assert "RuntimeError: boom" == failed.error
    with guard.hold("after-failure"):
        assert guard.owner == "after-failure"


def test_guard_reports_current_owner() -> None:
    guard = HardwareOperationGuard()
    with guard.hold("active-operation"):
        with pytest.raises(HardwareBusyError) as exc_info:
            guard.acquire("second-operation")
        assert exc_info.value.owner == "active-operation"
