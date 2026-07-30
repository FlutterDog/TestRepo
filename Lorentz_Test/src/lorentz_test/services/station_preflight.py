"""Check whether configured test-station endpoints can be acquired by the utility."""

from __future__ import annotations

import socket
from collections.abc import Callable
from contextlib import closing
from urllib.parse import urlparse

import serial

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import EndpointAccessResult, EndpointAccessStatus

SerialOpener = Callable[..., object]
TcpConnector = Callable[..., socket.socket]


def _serial_error_status(message: str) -> EndpointAccessStatus:
    lowered = message.casefold()
    busy_tokens = (
        "access is denied",
        "permission denied",
        "permissionerror",
        "errno 13",
        "resource busy",
        "in use",
    )
    missing_tokens = (
        "file not found",
        "cannot find",
        "no such file",
        "does not exist",
    )
    if any(token in lowered for token in busy_tokens):
        return EndpointAccessStatus.BUSY
    if any(token in lowered for token in missing_tokens):
        return EndpointAccessStatus.NOT_FOUND
    return EndpointAccessStatus.ERROR


def _probe_serial(
    name: str,
    endpoint: str,
    baudrate: int,
    serial_opener: SerialOpener,
) -> EndpointAccessResult:
    handle: object | None = None
    try:
        handle = serial_opener(
            port=endpoint,
            baudrate=baudrate,
            timeout=0.1,
            write_timeout=0.1,
        )
        return EndpointAccessResult(
            name=name,
            endpoint=endpoint,
            status=EndpointAccessStatus.AVAILABLE,
            detail="serial endpoint opened and released",
        )
    except (serial.SerialException, OSError) as exc:
        detail = f"{type(exc).__name__}: {exc}"
        return EndpointAccessResult(
            name=name,
            endpoint=endpoint,
            status=_serial_error_status(detail),
            detail=detail,
        )
    finally:
        close = getattr(handle, "close", None)
        if callable(close):
            close()


def _probe_tcp(
    name: str,
    endpoint: str,
    tcp_connector: TcpConnector,
) -> EndpointAccessResult:
    parsed = urlparse(endpoint)
    if not parsed.hostname or parsed.port is None:
        return EndpointAccessResult(
            name=name,
            endpoint=endpoint,
            status=EndpointAccessStatus.UNSUPPORTED,
            detail="expected tcp://host:port",
        )
    try:
        with closing(tcp_connector((parsed.hostname, parsed.port), timeout=0.75)):
            pass
        return EndpointAccessResult(
            name=name,
            endpoint=endpoint,
            status=EndpointAccessStatus.AVAILABLE,
            detail="TCP endpoint accepted a connection",
        )
    except OSError as exc:
        return EndpointAccessResult(
            name=name,
            endpoint=endpoint,
            status=EndpointAccessStatus.UNREACHABLE,
            detail=f"{type(exc).__name__}: {exc}",
        )


def probe_endpoint_access(
    name: str,
    endpoint: str | None,
    *,
    baudrate: int,
    serial_opener: SerialOpener = serial.Serial,
    tcp_connector: TcpConnector = socket.create_connection,
) -> EndpointAccessResult:
    if not endpoint:
        return EndpointAccessResult(
            name=name,
            endpoint=None,
            status=EndpointAccessStatus.SKIPPED,
            detail="endpoint not configured",
        )

    normalized = endpoint.strip()
    lowered = normalized.casefold()
    if lowered.startswith("tcp://"):
        return _probe_tcp(name, normalized, tcp_connector)
    if lowered.startswith("com") or lowered.startswith("\\\\.\\com"):
        return _probe_serial(name, normalized, baudrate, serial_opener)
    return EndpointAccessResult(
        name=name,
        endpoint=normalized,
        status=EndpointAccessStatus.UNSUPPORTED,
        detail="supported endpoint formats: COMx or tcp://host:port",
    )


def probe_station_endpoints(
    station: StationConfig,
    *,
    serial_opener: SerialOpener = serial.Serial,
    tcp_connector: TcpConnector = socket.create_connection,
) -> list[EndpointAccessResult]:
    endpoints = (
        ("S1", station.s1_endpoint),
        ("S2", station.s2_endpoint),
        ("S3", station.s3_endpoint),
        ("S4", station.s4_endpoint),
        ("HMI", station.hmi_endpoint),
        ("X2X adapter", station.x2x_endpoint),
    )
    return [
        probe_endpoint_access(
            name,
            endpoint,
            baudrate=station.serial_baudrate,
            serial_opener=serial_opener,
            tcp_connector=tcp_connector,
        )
        for name, endpoint in endpoints
    ]
