"""Sequential safe hardware verification and aggregate reporting."""

from __future__ import annotations

import time
from collections.abc import Callable
from datetime import datetime, timezone
from typing import Any, Protocol
from uuid import uuid4

from lorentz_test.models.full_test import (
    FullTestStageResult,
    FullTestStatus,
    FullTestSummary,
    HardwareVerificationPoint,
    LcpFullTestResult,
)
from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpHelloRequest, TestStatus
from lorentz_test.reporting.json_report import report_writer
from lorentz_test.services.lcp_active_ethernet import run_lcp_active_ethernet
from lorentz_test.services.lcp_active_rs485 import run_lcp_active_rs485
from lorentz_test.services.lcp_active_services import run_lcp_active_services
from lorentz_test.services.lcp_diagnostics import run_lcp_diagnostic_snapshot
from lorentz_test.services.lcp_hello import run_lcp_hello
from lorentz_test.services.lcp_hmi import run_lcp_hmi_echo


Runner = Callable[[LcpHelloRequest, StationConfig], Any]
ProgressCallback = Callable[[str, int, str, str, FullTestStageResult | None], None]


class FullTestReportWriter(Protocol):
    def save_lcp_hello(self, result: Any) -> object: ...
    def save_lcp_diagnostics(self, result: Any) -> object: ...
    def save_lcp_active_rs485(self, result: Any) -> object: ...
    def save_lcp_active_ethernet(self, result: Any) -> object: ...
    def save_lcp_active_services(self, result: Any) -> object: ...
    def save_lcp_hmi(self, result: Any) -> object: ...


_STAGE_META = (
    ("hello", "USB и firmware", "save_lcp_hello"),
    ("diagnostics", "Пассивная диагностика", "save_lcp_diagnostics"),
    ("rs485", "RS-485 S1–S4 и X2X", "save_lcp_active_rs485"),
    ("ethernet", "Ethernet ETH1/ETH2", "save_lcp_active_ethernet"),
    ("services", "Config, microSD, RTC и RTOS", "save_lcp_active_services"),
    ("hmi", "HMI echo", "save_lcp_hmi"),
)


def full_test_stage_definitions() -> tuple[tuple[str, str], ...]:
    return tuple((key, title) for key, title, _ in _STAGE_META)


def _to_full_status(status: TestStatus) -> FullTestStatus:
    if status == TestStatus.PASS:
        return FullTestStatus.PASS
    if status == TestStatus.FAIL:
        return FullTestStatus.FAIL
    if status == TestStatus.FIXTURE_ERROR:
        return FullTestStatus.FIXTURE_ERROR
    return FullTestStatus.INCOMPLETE


def _nested_statuses(stage_key: str, result: Any) -> list[TestStatus]:
    if stage_key in {"rs485", "ethernet"}:
        return [item.status for item in getattr(result, "interfaces", [])]
    if stage_key == "services":
        return [item.status for item in getattr(result, "steps", [])]
    return []


def _effective_stage_status(stage_key: str, result: Any) -> FullTestStatus:
    top = _to_full_status(result.result)
    if top != FullTestStatus.PASS:
        return top

    nested = _nested_statuses(stage_key, result)
    if TestStatus.FAIL in nested:
        return FullTestStatus.FAIL
    if TestStatus.FIXTURE_ERROR in nested:
        return FullTestStatus.FIXTURE_ERROR
    if TestStatus.SKIPPED in nested:
        return FullTestStatus.INCOMPLETE
    return FullTestStatus.PASS


def _stage_detail(result: Any) -> str:
    error = getattr(result, "error", None)
    if error:
        return str(error)
    detail = getattr(result, "detail", None)
    if detail:
        return str(detail)
    note = getattr(result, "note", None)
    if note:
        return str(note)
    return f"result={result.result.value}"


def _execute_stage(
    key: str,
    title: str,
    saver_name: str,
    runner: Runner,
    request: LcpHelloRequest,
    station: StationConfig,
    writer: FullTestReportWriter,
) -> tuple[FullTestStageResult, Any | None]:
    stage_started = datetime.now(timezone.utc)
    started = time.monotonic()
    try:
        result = runner(request, station)
    except Exception as exc:
        duration_ms = round((time.monotonic() - started) * 1000)
        return (
            FullTestStageResult(
                key=key,
                title=title,
                raw_result=TestStatus.FAIL,
                effective_result=FullTestStatus.FAIL,
                started_at=stage_started,
                duration_ms=duration_ms,
                detail=f"Unhandled {type(exc).__name__}: {exc}",
                error=f"{type(exc).__name__}: {exc}",
            ),
            None,
        )

    report_error: str | None = None
    try:
        getattr(writer, saver_name)(result)
    except Exception as exc:
        report_error = f"report save failed: {type(exc).__name__}: {exc}"

    effective = _effective_stage_status(key, result)
    if report_error and effective != FullTestStatus.FAIL:
        effective = FullTestStatus.FIXTURE_ERROR

    detail = _stage_detail(result)
    if report_error:
        detail = f"{detail}; {report_error}"

    return (
        FullTestStageResult(
            key=key,
            title=title,
            raw_result=result.result,
            effective_result=effective,
            started_at=getattr(result, "started_at", stage_started),
            duration_ms=getattr(
                result,
                "duration_ms",
                round((time.monotonic() - started) * 1000),
            ),
            test_id=getattr(result, "test_id", None),
            report_file=getattr(result, "report_file", None),
            detail=detail,
            error=getattr(result, "error", None) or report_error,
        ),
        result,
    )


def _blocked_stage(key: str, title: str, blocked_by: str) -> FullTestStageResult:
    return FullTestStageResult(
        key=key,
        title=title,
        raw_result=TestStatus.SKIPPED,
        effective_result=FullTestStatus.INCOMPLETE,
        started_at=datetime.now(timezone.utc),
        duration_ms=0,
        detail=f"Этап не запущен: критический prerequisite {blocked_by} не прошёл.",
        blocked_by=blocked_by,
    )


def _point(
    *,
    key: str,
    title: str,
    group: str,
    status: TestStatus,
    configured: bool,
    stage_key: str,
    detail: str,
    report_file: str | None,
) -> HardwareVerificationPoint:
    return HardwareVerificationPoint(
        key=key,
        title=title,
        group=group,
        result=status,
        evaluated=status in {TestStatus.PASS, TestStatus.FAIL},
        configured=configured,
        source_stage=stage_key,
        detail=detail,
        report_file=report_file,
    )


def _fallback_status(
    stage: FullTestStageResult | None,
    *,
    configured: bool,
) -> TestStatus:
    if not configured:
        return TestStatus.SKIPPED
    if stage is None:
        return TestStatus.SKIPPED
    if stage.raw_result in {TestStatus.FAIL, TestStatus.FIXTURE_ERROR}:
        return stage.raw_result
    return TestStatus.SKIPPED


def _build_hardware_points(
    station: StationConfig,
    stages: list[FullTestStageResult],
    results: dict[str, Any],
) -> list[HardwareVerificationPoint]:
    stage_by_key = {stage.key: stage for stage in stages}
    points: list[HardwareVerificationPoint] = []

    hello_stage = stage_by_key.get("hello")
    points.append(
        _point(
            key="usb_firmware",
            title="USB transport и firmware",
            group="Core",
            status=hello_stage.raw_result if hello_stage else TestStatus.SKIPPED,
            configured=bool(station.lcp_port),
            stage_key="hello",
            detail=hello_stage.detail if hello_stage else "этап отсутствует",
            report_file=hello_stage.report_file if hello_stage else None,
        )
    )

    diagnostics_stage = stage_by_key.get("diagnostics")
    points.append(
        _point(
            key="internal_diagnostics",
            title="Внутренняя диагностика LCP",
            group="Core",
            status=(
                diagnostics_stage.raw_result
                if diagnostics_stage
                else TestStatus.SKIPPED
            ),
            configured=bool(station.lcp_port),
            stage_key="diagnostics",
            detail=diagnostics_stage.detail if diagnostics_stage else "этап отсутствует",
            report_file=diagnostics_stage.report_file if diagnostics_stage else None,
        )
    )

    rs_stage = stage_by_key.get("rs485")
    rs_result = results.get("rs485")
    rs_items = {item.name: item for item in getattr(rs_result, "interfaces", [])}
    rs_config = {
        "S1": station.s1_endpoint,
        "S2": station.s2_endpoint,
        "S3": station.s3_endpoint,
        "S4": station.s4_endpoint,
        "X2X": station.x2x_endpoint,
    }
    for name, endpoint in rs_config.items():
        item = rs_items.get(name)
        status = (
            item.status
            if item is not None
            else _fallback_status(rs_stage, configured=bool(endpoint))
        )
        detail = (
            item.detail
            if item is not None
            else (
                rs_stage.detail
                if rs_stage and status != TestStatus.SKIPPED
                else "endpoint не настроен или этап заблокирован"
            )
        )
        points.append(
            _point(
                key=name.casefold(),
                title=name,
                group="RS-485",
                status=status,
                configured=bool(endpoint),
                stage_key="rs485",
                detail=detail,
                report_file=rs_stage.report_file if rs_stage else None,
            )
        )

    eth_stage = stage_by_key.get("ethernet")
    eth_result = results.get("ethernet")
    eth_items = {item.name: item for item in getattr(eth_result, "interfaces", [])}
    eth_config = {
        "ETH1": station.eth1_test_enabled,
        "ETH2": station.eth2_test_enabled,
    }
    for name, enabled in eth_config.items():
        item = eth_items.get(name)
        status = (
            item.status
            if item is not None
            else _fallback_status(eth_stage, configured=enabled)
        )
        detail = (
            item.detail
            if item is not None
            else (
                eth_stage.detail
                if eth_stage and status != TestStatus.SKIPPED
                else "Ethernet test disabled or stage blocked"
            )
        )
        points.append(
            _point(
                key=name.casefold(),
                title=name,
                group="Ethernet",
                status=status,
                configured=enabled,
                stage_key="ethernet",
                detail=detail,
                report_file=eth_stage.report_file if eth_stage else None,
            )
        )

    services_stage = stage_by_key.get("services")
    services_result = results.get("services")
    service_items = {
        item.name: item for item in getattr(services_result, "steps", [])
    }
    service_map = (
        ("config_transport", "USB configuration transport", "USB configuration read/validate"),
        ("microsd", "microSD write/read", "microSD write/read"),
        ("rtc", "RTC synchronization and tick", "RTC synchronization and tick"),
        ("rtos", "RTOS short stability", "RTOS short stability"),
    )
    for key, title, result_name in service_map:
        item = service_items.get(result_name)
        status = (
            item.status
            if item is not None
            else _fallback_status(services_stage, configured=True)
        )
        detail = (
            item.detail
            if item is not None
            else (
                services_stage.detail
                if services_stage and status != TestStatus.SKIPPED
                else "этап заблокирован"
            )
        )
        points.append(
            _point(
                key=key,
                title=title,
                group="Internal services",
                status=status,
                configured=True,
                stage_key="services",
                detail=detail,
                report_file=services_stage.report_file if services_stage else None,
            )
        )

    hmi_stage = stage_by_key.get("hmi")
    hmi_result = results.get("hmi")
    hmi_status = (
        hmi_result.result
        if hmi_result is not None
        else _fallback_status(hmi_stage, configured=bool(station.hmi_endpoint))
    )
    hmi_detail = (
        hmi_result.detail
        if hmi_result is not None
        else (
            hmi_stage.detail
            if hmi_stage and hmi_status != TestStatus.SKIPPED
            else "HMI endpoint не настроен или этап заблокирован"
        )
    )
    points.append(
        _point(
            key="hmi",
            title="HMI echo",
            group="RS-485",
            status=hmi_status,
            configured=bool(station.hmi_endpoint),
            stage_key="hmi",
            detail=hmi_detail,
            report_file=hmi_stage.report_file if hmi_stage else None,
        )
    )
    return points


def _summary(points: list[HardwareVerificationPoint]) -> FullTestSummary:
    passed = [point for point in points if point.result == TestStatus.PASS]
    failed = [point for point in points if point.result == TestStatus.FAIL]
    fixture = [
        point for point in points if point.result == TestStatus.FIXTURE_ERROR
    ]
    skipped = [point for point in points if point.result == TestStatus.SKIPPED]
    return FullTestSummary(
        total_points=len(points),
        evaluated_points=len(passed) + len(failed),
        passed_points=len(passed),
        failed_points=len(failed),
        fixture_error_points=len(fixture),
        skipped_points=len(skipped),
        pending_points=[point.title for point in fixture + skipped],
        failed_point_names=[point.title for point in failed],
    )


def _overall(stages: list[FullTestStageResult]) -> FullTestStatus:
    statuses = [stage.effective_result for stage in stages]
    if FullTestStatus.FAIL in statuses:
        return FullTestStatus.FAIL
    if FullTestStatus.FIXTURE_ERROR in statuses:
        return FullTestStatus.FIXTURE_ERROR
    if FullTestStatus.INCOMPLETE in statuses:
        return FullTestStatus.INCOMPLETE
    return FullTestStatus.PASS


def run_lcp_full_test(
    request: LcpHelloRequest,
    station: StationConfig,
    *,
    run_id: str | None = None,
    progress_callback: ProgressCallback | None = None,
    hello_runner: Runner = run_lcp_hello,
    diagnostics_runner: Runner = run_lcp_diagnostic_snapshot,
    rs485_runner: Runner = run_lcp_active_rs485,
    ethernet_runner: Runner = run_lcp_active_ethernet,
    services_runner: Runner = run_lcp_active_services,
    hmi_runner: Runner = run_lcp_hmi_echo,
    writer: FullTestReportWriter = report_writer,
) -> LcpFullTestResult:
    """Run the complete safe LCP verification in a deterministic sequence."""
    started_at = datetime.now(timezone.utc)
    started = time.monotonic()
    stages: list[FullTestStageResult] = []
    results: dict[str, Any] = {}
    resolved_run_id = run_id or uuid4().hex

    runners: dict[str, Runner] = {
        "hello": hello_runner,
        "diagnostics": diagnostics_runner,
        "rs485": rs485_runner,
        "ethernet": ethernet_runner,
        "services": services_runner,
        "hmi": hmi_runner,
    }

    for index, (key, title, saver_name) in enumerate(_STAGE_META, start=1):
        if index > 1 and stages[0].effective_result != FullTestStatus.PASS:
            stage = _blocked_stage(key, title, "USB и firmware")
            stages.append(stage)
            if progress_callback:
                progress_callback("completed", index, key, title, stage)
            continue

        if progress_callback:
            progress_callback("started", index, key, title, None)

        stage, result = _execute_stage(
            key,
            title,
            saver_name,
            runners[key],
            request,
            station,
            writer,
        )
        stages.append(stage)
        if result is not None:
            results[key] = result
        if progress_callback:
            progress_callback("completed", index, key, title, stage)

    hardware = _build_hardware_points(station, stages, results)
    completed_at = datetime.now(timezone.utc)
    hello_result = results.get("hello")
    return LcpFullTestResult(
        run_id=resolved_run_id,
        result=_overall(stages),
        started_at=started_at,
        completed_at=completed_at,
        duration_ms=round((time.monotonic() - started) * 1000),
        station_name=station.station_name,
        station_snapshot=station.model_dump(mode="json"),
        serial_number=request.serial_number,
        operator=request.operator,
        port=request.port or station.lcp_port or "",
        firmware_version=getattr(hello_result, "firmware_version", None),
        stages=stages,
        hardware=hardware,
        summary=_summary(hardware),
    )
