"""Execution of the first real LCP2116 hardware identity test."""

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
from lorentz_test.protocols.lcp_console import FirmwareIdentity, LcpDiagnosticConsole
from lorentz_test.protocols.lcp_usb import HelloInfo, LcpUsbClient

ClientFactory = Callable[..., LcpUsbClient]
ConsoleFactory = Callable[..., LcpDiagnosticConsole]
_REQUIRED_CAPABILITIES = 0x0000000F
_EXPECTED_FIRMWARE_NAME = "LCP Basic Diagnostic Firmware"
_EXPECTED_TARGET = "ATSAM3X8E"
_CDC_CLOSE_SETTLE_SECONDS = 0.30


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


def _evaluate_firmware(
    identity: FirmwareIdentity,
    expected_version: str,
) -> list[CheckResult]:
    expected_stage = f"Release {expected_version}"
    return [
        _check(
            "firmware_name",
            _EXPECTED_FIRMWARE_NAME,
            identity.name,
            identity.name == _EXPECTED_FIRMWARE_NAME,
        ),
        _check(
            "firmware_version",
            expected_version,
            identity.version,
            identity.version == expected_version,
        ),
        _check(
            "firmware_stage",
            expected_stage,
            identity.stage,
            identity.stage == expected_stage,
        ),
        _check(
            "firmware_target",
            _EXPECTED_TARGET,
            identity.target,
            identity.target == _EXPECTED_TARGET,
        ),
    ]


def run_lcp_hello(
    request: LcpHelloRequest,
    station: StationConfig,
    *,
    client_factory: ClientFactory = LcpUsbClient,
    console_factory: ConsoleFactory = LcpDiagnosticConsole.open_port,
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
            expected_firmware_version=station.expected_firmware_version,
            error="LCP USB port is not configured",
        )

    client: LcpUsbClient | None = None
    console: LcpDiagnosticConsole | None = None
    hello_payload: HelloPayload | None = None
    checks: list[CheckResult] = []
    identity: FirmwareIdentity | None = None

    try:
        client = client_factory(
            port,
            baudrate=station.serial_baudrate,
            timeout=station.serial_timeout_seconds,
        )
        hello = client.hello()
        hello_payload = HelloPayload(**hello.to_dict())
        checks.extend(_evaluate_hello(hello))

        # EXIT is acknowledged before the firmware releases the binary parser.
        # Closing and reopening CDC forces a clean SerialUSB open-state transition
        # and prevents the first text command from being consumed by that parser.
        client.exit_binary_session()
        client.close()
        client = None
        time.sleep(_CDC_CLOSE_SETTLE_SECONDS)

        console = console_factory(
            port,
            baudrate=station.serial_baudrate,
            timeout=station.serial_timeout_seconds,
        )
        identity = console.read_firmware_identity()
        checks.extend(_evaluate_firmware(identity, station.expected_firmware_version))

        status = (
            TestStatus.PASS
            if all(item.status == TestStatus.PASS for item in checks)
            else TestStatus.FAIL
        )
        return LcpHelloResult(
            result=status,
            started_at=started_at,
            duration_ms=round((time.monotonic() - started) * 1000),
            station_name=station.station_name,
            serial_number=request.serial_number,
            operator=request.operator,
            port=port,
            hello=hello_payload,
            checks=checks,
            expected_firmware_version=station.expected_firmware_version,
            firmware_name=identity.name,
            firmware_version=identity.version,
            firmware_stage=identity.stage,
            firmware_target=identity.target,
            firmware_version_verified=(identity.version == station.expected_firmware_version),
            firmware_note="Версия firmware подтверждена командой version диагностической консоли.",
            firmware_raw_output=identity.raw_output,
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
            hello=hello_payload,
            checks=checks,
            expected_firmware_version=station.expected_firmware_version,
            firmware_name=identity.name if identity is not None else None,
            firmware_version=identity.version if identity is not None else None,
            firmware_stage=identity.stage if identity is not None else None,
            firmware_target=identity.target if identity is not None else None,
            firmware_version_verified=False,
            firmware_raw_output=identity.raw_output if identity is not None else None,
            error=f"{type(exc).__name__}: {exc}",
        )
    finally:
        if console is not None:
            console.close()
        if client is not None:
            client.close()