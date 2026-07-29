"""Execution of the first real LCP2116 hardware test."""

from __future__ import annotations

import time
from collections.abc import Callable
from datetime import datetime, timezone

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import (
    CheckResult,
    HelloPayload,
    LcpHelloRequest,
    LcpHelloResult,
    TestStatus,
)
from lorentz_test.protocols.lcp_usb import HelloInfo, LcpUsbClient

ClientFactory = Callable[..., LcpUsbClient]
_REQUIRED_CAPABILITIES = 0x0000000F


def _check(name: str, expected: object, actual: object, passed: bool) -> CheckResult:
    return CheckResult(
        name=name,
        expected=str(expected),
        actual=str(actual),
        status=TestStatus.PASS if passed else TestStatus.FAIL,
    )


def _evaluate_hello(hello: HelloInfo) -> list[CheckResult]:
    return [
        _check("protocol_version", 1, hello.protocol_version, hello.protocol_version == 1),
        _check("schema_version", 1, hello.schema_version, hello.schema_version == 1),
        _check("bundle_size", 104, hello.bundle_size, hello.bundle_size == 104),
        _check("max_payload", 160, hello.max_payload, hello.max_payload == 160),
        _check("storage_version", 1, hello.storage_version, hello.storage_version == 1),
        _check("header_size", 24, hello.header_size, hello.header_size == 24),
        _check(
            "capabilities",
            "read|validate|write|reboot (0x0000000F)",
            f"0x{hello.capabilities:08X}",
            (hello.capabilities & _REQUIRED_CAPABILITIES) == _REQUIRED_CAPABILITIES,
        ),
    ]


def run_lcp_hello(
    request: LcpHelloRequest,
    station: StationConfig,
    *,
    client_factory: ClientFactory = LcpUsbClient,
) -> LcpHelloResult:
    port = request.port or station.lcp_port
    started_at = datetime.now(timezone.utc)
    started = time.monotonic()

    if not port:
        return LcpHelloResult(
            result=TestStatus.FAIL,
            started_at=started_at,
            duration_ms=0,
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port="",
            error="LCP USB port is not configured",
        )

    client: LcpUsbClient | None = None
    try:
        client = client_factory(
            port,
            baudrate=station.serial_baudrate,
            timeout=station.serial_timeout_seconds,
        )
        hello = client.hello()
        checks = _evaluate_hello(hello)
        status = TestStatus.PASS if all(item.status == TestStatus.PASS for item in checks) else TestStatus.FAIL
        return LcpHelloResult(
            result=status,
            started_at=started_at,
            duration_ms=round((time.monotonic() - started) * 1000),
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port=port,
            hello=HelloPayload(**hello.to_dict()),
            checks=checks,
        )
    except Exception as exc:
        return LcpHelloResult(
            result=TestStatus.FAIL,
            started_at=started_at,
            duration_ms=round((time.monotonic() - started) * 1000),
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port=port,
            error=f"{type(exc).__name__}: {exc}",
        )
    finally:
        if client is not None:
            client.close()
