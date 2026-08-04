"""Fast, non-destructive readiness check for an LCP2116 test station."""

from __future__ import annotations

import socket
import time
from collections.abc import Callable
from contextlib import closing
from datetime import datetime, timezone
from pathlib import Path
from uuid import uuid4

from lorentz_test.models.precheck import (
    LcpPrecheckResult,
    PrecheckCheck,
    PrecheckCheckStatus,
    PrecheckStatus,
)
from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import (
    EndpointAccessResult,
    EndpointAccessStatus,
    LcpHelloRequest,
    LcpHelloResult,
    TestStatus,
)
from lorentz_test.paths import data_dir, reports_dir
from lorentz_test.services.lcp_hello import run_lcp_hello
from lorentz_test.services.station_preflight import probe_station_endpoints

HelloRunner = Callable[[LcpHelloRequest, StationConfig], LcpHelloResult]
EndpointProbe = Callable[[StationConfig], list[EndpointAccessResult]]
TcpConnector = Callable[..., socket.socket]
PathFactory = Callable[[], Path]


def _check(
    key: str,
    title: str,
    status: PrecheckCheckStatus,
    detail: str,
    *,
    blocking: bool,
) -> PrecheckCheck:
    return PrecheckCheck(
        key=key,
        title=title,
        status=status,
        blocking=blocking,
        detail=detail,
    )


def _endpoint_check(item: EndpointAccessResult) -> PrecheckCheck:
    key = f"endpoint_{item.name.casefold().replace(' ', '_')}"
    title = f"Endpoint {item.name}"
    if item.status == EndpointAccessStatus.AVAILABLE:
        return _check(key, title, PrecheckCheckStatus.PASS, item.detail, blocking=False)
    if item.status == EndpointAccessStatus.SKIPPED:
        return _check(
            key,
            title,
            PrecheckCheckStatus.WARNING,
            "Интерфейс не настроен и будет пропущен в полном тесте.",
            blocking=False,
        )
    return _check(key, title, PrecheckCheckStatus.FAIL, item.detail, blocking=True)


def _probe_ethernet(
    name: str,
    target_ip: object,
    source_ip: object | None,
    enabled: bool,
    tcp_connector: TcpConnector,
) -> PrecheckCheck:
    key = name.casefold()
    if not enabled:
        return _check(
            key,
            name,
            PrecheckCheckStatus.SKIPPED,
            "Активный Ethernet-тест отключён в настройках стенда.",
            blocking=False,
        )

    target = (str(target_ip), 502)
    kwargs: dict[str, object] = {"timeout": 1.0}
    if source_ip is not None:
        kwargs["source_address"] = (str(source_ip), 0)
    try:
        with closing(tcp_connector(target, **kwargs)):
            pass
        source = str(source_ip) if source_ip is not None else "системный маршрут"
        return _check(
            key,
            name,
            PrecheckCheckStatus.PASS,
            f"TCP {target[0]}:502 доступен; source={source}.",
            blocking=False,
        )
    except OSError as exc:
        return _check(
            key,
            name,
            PrecheckCheckStatus.FAIL,
            f"TCP {target[0]}:502 недоступен; {type(exc).__name__}: {exc}",
            blocking=True,
        )


def _probe_writable_directory(key: str, title: str, path_factory: PathFactory) -> PrecheckCheck:
    probe_path: Path | None = None
    try:
        directory = path_factory()
        probe_path = directory / f".lorentz-precheck-{uuid4().hex}.tmp"
        probe_path.write_text("ok", encoding="utf-8")
        probe_path.unlink()
        probe_path = None
        return _check(
            key,
            title,
            PrecheckCheckStatus.PASS,
            f"Каталог доступен для записи: {directory}",
            blocking=False,
        )
    except OSError as exc:
        return _check(
            key,
            title,
            PrecheckCheckStatus.FAIL,
            f"Каталог недоступен для записи; {type(exc).__name__}: {exc}",
            blocking=True,
        )
    finally:
        if probe_path is not None:
            try:
                probe_path.unlink(missing_ok=True)
            except OSError:
                pass


def _shared_adapter_check(station: StationConfig) -> PrecheckCheck | None:
    if not station.shared_hmi_x2x_adapter:
        return None
    hmi = station.hmi_endpoint.casefold() if station.hmi_endpoint else None
    x2x = station.x2x_endpoint.casefold() if station.x2x_endpoint else None
    if hmi is not None and x2x is not None and hmi == x2x:
        return _check(
            "shared_hmi_x2x",
            "Общий адаптер HMI/X2X",
            PrecheckCheckStatus.WARNING,
            "Один endpoint используется последовательно: X2X войдёт в full-run, HMI потребуется запустить после перестановки адаптера.",
            blocking=False,
        )
    return _check(
        "shared_hmi_x2x",
        "Общий адаптер HMI/X2X",
        PrecheckCheckStatus.WARNING,
        "Флаг общего адаптера включён, но HMI и X2X не указывают один и тот же endpoint. Для разных адаптеров выключите этот флаг.",
        blocking=False,
    )


def run_lcp_precheck(
    request: LcpHelloRequest,
    station: StationConfig,
    *,
    hello_runner: HelloRunner = run_lcp_hello,
    endpoint_probe: EndpointProbe = probe_station_endpoints,
    tcp_connector: TcpConnector = socket.create_connection,
    reports_path_factory: PathFactory = reports_dir,
    runtime_path_factory: PathFactory = data_dir,
) -> LcpPrecheckResult:
    """Verify station readiness without modifying DUT Flash or persistent configuration."""

    started_at = datetime.now(timezone.utc)
    started = time.monotonic()
    port = request.port or station.lcp_port or ""
    checks: list[PrecheckCheck] = []
    firmware_version: str | None = None

    try:
        hello = hello_runner(request, station)
        firmware_version = hello.firmware_version
        if hello.result == TestStatus.PASS:
            checks.append(
                _check(
                    "usb_firmware",
                    "USB и firmware",
                    PrecheckCheckStatus.PASS,
                    f"LCP доступен на {hello.port}; firmware={hello.firmware_version}.",
                    blocking=False,
                )
            )
        else:
            detail = hello.error or "USB/firmware verification failed"
            checks.append(
                _check(
                    "usb_firmware",
                    "USB и firmware",
                    PrecheckCheckStatus.FAIL,
                    detail,
                    blocking=True,
                )
            )

        checks.extend(_endpoint_check(item) for item in endpoint_probe(station))
        checks.append(
            _probe_ethernet(
                "ETH1",
                station.eth1_ip,
                station.eth1_source_ip,
                station.eth1_test_enabled,
                tcp_connector,
            )
        )
        checks.append(
            _probe_ethernet(
                "ETH2",
                station.eth2_ip,
                station.eth2_source_ip,
                station.eth2_test_enabled,
                tcp_connector,
            )
        )
        checks.append(
            _probe_writable_directory(
                "reports_directory",
                "Каталог отчётов",
                reports_path_factory,
            )
        )
        checks.append(
            _probe_writable_directory(
                "runtime_directory",
                "Runtime-каталог",
                runtime_path_factory,
            )
        )
        shared = _shared_adapter_check(station)
        if shared is not None:
            checks.append(shared)
    except Exception as exc:
        checks.append(
            _check(
                "precheck_internal",
                "Внутренняя ошибка pre-check",
                PrecheckCheckStatus.FAIL,
                f"{type(exc).__name__}: {exc}",
                blocking=True,
            )
        )

    blocking_failures = [item.title for item in checks if item.blocking and item.status == PrecheckCheckStatus.FAIL]
    warnings = [item.title for item in checks if item.status == PrecheckCheckStatus.WARNING]
    ready = not blocking_failures
    return LcpPrecheckResult(
        result=PrecheckStatus.READY if ready else PrecheckStatus.NOT_READY,
        ready=ready,
        started_at=started_at,
        duration_ms=round((time.monotonic() - started) * 1000),
        station_name=station.station_name,
        serial_number=request.serial_number,
        operator=request.operator,
        port=port,
        firmware_version=firmware_version,
        checks=checks,
        blocking_failures=blocking_failures,
        warnings=warnings,
    )
