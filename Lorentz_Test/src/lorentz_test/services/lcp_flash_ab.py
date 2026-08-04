"""Destructive but self-restoring configuration Flash A/B test."""

from __future__ import annotations

import json
import os
import re
import struct
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import (
    ActiveStepResult,
    CheckResult,
    ConfirmedTestRequest,
    LcpFlashAbResult,
    TestStatus,
)
from lorentz_test.paths import reports_dir
from lorentz_test.protocols.lcp_reconnect import (
    PortIdentity,
    capture_port_identity,
    wait_for_lcp_reconnect,
)
from lorentz_test.protocols.lcp_usb import (
    COMMAND_GET_CONFIG,
    COMMAND_GET_STATUS,
    COMMAND_PUT_CONFIG,
    COMMAND_REBOOT,
    COMMAND_VALIDATE_CONFIG,
    STATUS_ACCEPTED,
    STATUS_NO_CHANGE,
    STATUS_OK,
    STATUS_REBOOT_REQUIRED,
    LcpUsbClient,
    ProtocolError,
    crc32,
)

WRITER_COMPLETE = 4
WRITER_ERROR = 5
_SAFE = re.compile(r"[^A-Za-z0-9._-]+")


@dataclass(frozen=True)
class BundleSnapshot:
    schema: int
    bundle_size: int
    sequence: int
    generation: int
    stored_crc32: int
    source: int
    slot: int
    bundle: bytes


@dataclass(frozen=True)
class WriterSnapshot:
    state: int
    result: int
    active_slot: int
    target_slot: int
    service_busy: bool
    active_sequence: int
    target_sequence: int
    active_crc32: int
    generation: int
    flash_result: int


def _check(name: str, expected: str, actual: object, passed: bool) -> CheckResult:
    return CheckResult(
        name=name,
        expected=expected,
        actual=str(actual),
        status=TestStatus.PASS if passed else TestStatus.FAIL,
    )


def _read_bundle(client: LcpUsbClient) -> BundleSnapshot:
    frame = client.request(COMMAND_GET_CONFIG, accepted={STATUS_OK})
    if len(frame.payload) != 124:
        raise ProtocolError(f"GET_CONFIG length {len(frame.payload)} != 124")
    schema, bundle_size = struct.unpack_from("<HH", frame.payload, 0)
    sequence, generation, stored_crc = struct.unpack_from("<III", frame.payload, 4)
    source = frame.payload[16]
    slot = frame.payload[17]
    bundle = frame.payload[20:]
    if schema != 1 or bundle_size != 104 or len(bundle) != 104:
        raise ProtocolError(
            f"unsupported bundle schema={schema}, size={bundle_size}, payload={len(bundle)}"
        )
    calculated = crc32(bundle)
    if calculated != stored_crc:
        raise ProtocolError(
            f"active bundle CRC 0x{calculated:08X} != stored 0x{stored_crc:08X}"
        )
    return BundleSnapshot(
        schema,
        bundle_size,
        sequence,
        generation,
        stored_crc,
        source,
        slot,
        bundle,
    )


def _writer_status(client: LcpUsbClient) -> WriterSnapshot:
    frame = client.request(COMMAND_GET_STATUS, accepted={STATUS_OK})
    if len(frame.payload) != 28:
        raise ProtocolError(f"GET_STATUS length {len(frame.payload)} != 28")
    return WriterSnapshot(
        state=frame.payload[0],
        result=frame.payload[1],
        active_slot=frame.payload[3],
        target_slot=frame.payload[4],
        service_busy=bool(frame.payload[5]),
        active_sequence=struct.unpack_from("<I", frame.payload, 8)[0],
        target_sequence=struct.unpack_from("<I", frame.payload, 12)[0],
        active_crc32=struct.unpack_from("<I", frame.payload, 16)[0],
        generation=struct.unpack_from("<I", frame.payload, 20)[0],
        flash_result=frame.payload[24],
    )


def _payload(bundle: bytes) -> bytes:
    if len(bundle) != 104:
        raise ValueError(f"bundle length {len(bundle)} != 104")
    return struct.pack("<HHI", 1, len(bundle), crc32(bundle)) + bundle


def _write_bundle(client: LcpUsbClient, bundle: bytes, timeout: float = 8.0) -> WriterSnapshot:
    payload = _payload(bundle)
    client.request(COMMAND_VALIDATE_CONFIG, payload, accepted={STATUS_OK})
    response = client.request(
        COMMAND_PUT_CONFIG,
        payload,
        accepted={STATUS_ACCEPTED, STATUS_NO_CHANGE},
    )
    if response.status == STATUS_NO_CHANGE:
        raise ProtocolError("PUT_CONFIG returned NO_CHANGE; A/B slot switch was not exercised")

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        writer = _writer_status(client)
        if writer.state == WRITER_COMPLETE:
            if writer.result != STATUS_REBOOT_REQUIRED or writer.flash_result != 0:
                raise ProtocolError(
                    f"Flash writer completed with result={writer.result}, "
                    f"flash_result={writer.flash_result}"
                )
            return writer
        if writer.state == WRITER_ERROR:
            raise ProtocolError(
                f"Flash writer error result={writer.result}, flash_result={writer.flash_result}"
            )
        time.sleep(0.05)
    raise ProtocolError("timeout waiting for Flash A/B commit")


def _reboot(client: LcpUsbClient) -> None:
    client.request(COMMAND_REBOOT, accepted={STATUS_OK}, timeout=1.0)


def _read_from_port(port: str, station: StationConfig) -> BundleSnapshot:
    with LcpUsbClient(
        port,
        baudrate=station.serial_baudrate,
        timeout=station.serial_timeout_seconds,
    ) as client:
        client.hello()
        return _read_bundle(client)


def _write_and_reconnect(
    port: str,
    identity: PortIdentity,
    station: StationConfig,
    bundle: bytes,
) -> tuple[str, WriterSnapshot]:
    with LcpUsbClient(
        port,
        baudrate=station.serial_baudrate,
        timeout=station.serial_timeout_seconds,
    ) as client:
        client.hello()
        writer = _write_bundle(client, bundle)
        _reboot(client)

    reconnected = wait_for_lcp_reconnect(
        port,
        identity,
        baudrate=station.serial_baudrate,
        serial_timeout=station.serial_timeout_seconds,
        total_timeout=35.0,
    )
    return reconnected, writer


def _backup_path(serial_number: str, started_at: datetime) -> Path:
    safe = _SAFE.sub("_", serial_number).strip("._-") or "unknown"
    return reports_dir() / f"LCP2116_{safe}_{started_at:%Y%m%d_%H%M%S}_FLASH_RECOVERY.json"


def _write_backup(path: Path, snapshot: BundleSnapshot, test_bundle: bytes) -> None:
    data = {
        "schema_version": snapshot.schema,
        "bundle_size": snapshot.bundle_size,
        "sequence": snapshot.sequence,
        "generation": snapshot.generation,
        "source": snapshot.source,
        "slot": snapshot.slot,
        "original_crc32": f"0x{snapshot.stored_crc32:08X}",
        "original_bundle_hex": snapshot.bundle.hex(),
        "test_crc32": f"0x{crc32(test_bundle):08X}",
        "test_bundle_hex": test_bundle.hex(),
        "recovery_instruction": "Write original_bundle_hex through PUT_CONFIG and reboot.",
    }
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def _test_bundle(original: bytes) -> tuple[bytes, int, int]:
    output = bytearray(original)
    original_s4_baud = struct.unpack_from("<I", output, 12)[0]
    test_s4_baud = 19200 if original_s4_baud != 19200 else 9600
    struct.pack_into("<I", output, 12, test_s4_baud)
    return bytes(output), original_s4_baud, test_s4_baud


def run_lcp_flash_ab(
    request: ConfirmedTestRequest,
    station: StationConfig,
) -> LcpFlashAbResult:
    """Exercise both Flash slots and restore the exact original 104-byte bundle."""
    port = request.port or station.lcp_port or ""
    started_at = datetime.now(timezone.utc)
    started = time.monotonic()
    result = LcpFlashAbResult(
        result=TestStatus.RUNNING,
        started_at=started_at,
        duration_ms=0,
        station_name=station.station_name,
        serial_number=request.serial_number,
        operator=request.operator,
        port=port,
    )
    if request.confirmation != "FLASH A/B":
        result.result = TestStatus.SKIPPED
        result.error = "confirmation must be exactly FLASH A/B"
        return result
    if not port:
        result.result = TestStatus.FIXTURE_ERROR
        result.error = "LCP USB port is not configured"
        return result

    identity = capture_port_identity(port)
    current_port = port
    original: BundleSnapshot | None = None
    test_active_possible = False

    try:
        step_started = time.monotonic()
        original = _read_from_port(current_port, station)
        test_bundle, original_s4, test_s4 = _test_bundle(original.bundle)
        backup = _backup_path(request.serial_number, started_at)
        _write_backup(backup, original, test_bundle)
        result.backup_file = str(backup.resolve())
        result.original_slot = original.slot
        result.original_crc32 = f"0x{original.stored_crc32:08X}"
        result.test_crc32 = f"0x{crc32(test_bundle):08X}"
        result.steps.append(
            ActiveStepResult(
                name="Backup original configuration",
                status=TestStatus.PASS,
                duration_ms=round((time.monotonic() - step_started) * 1000),
                detail=(
                    f"slot={original.slot}, sequence={original.sequence}, "
                    f"S4 baud {original_s4}->{test_s4}; recovery file written before mutation"
                ),
            )
        )

        step_started = time.monotonic()
        test_active_possible = True
        current_port, writer = _write_and_reconnect(
            current_port,
            identity,
            station,
            test_bundle,
        )
        result.reconnected_port = current_port
        test_snapshot = _read_from_port(current_port, station)
        result.test_slot = test_snapshot.slot
        test_pass = (
            test_snapshot.bundle == test_bundle
            and test_snapshot.stored_crc32 == crc32(test_bundle)
            and test_snapshot.slot != original.slot
            and writer.target_slot == test_snapshot.slot
        )
        result.steps.append(
            ActiveStepResult(
                name="Write test bundle and boot alternate slot",
                status=TestStatus.PASS if test_pass else TestStatus.FAIL,
                duration_ms=round((time.monotonic() - step_started) * 1000),
                detail=(
                    f"original slot={original.slot}, writer target={writer.target_slot}, "
                    f"active test slot={test_snapshot.slot}, crc=0x{test_snapshot.stored_crc32:08X}"
                ),
                checks=[
                    _check("test_bundle_read_back", "exact match", "match" if test_snapshot.bundle == test_bundle else "mismatch", test_snapshot.bundle == test_bundle),
                    _check("alternate_slot", f"not {original.slot}", test_snapshot.slot, test_snapshot.slot != original.slot),
                ],
            )
        )
        if not test_pass:
            raise ProtocolError("test bundle did not become the exact active alternate slot")

        step_started = time.monotonic()
        current_port, restore_writer = _write_and_reconnect(
            current_port,
            identity,
            station,
            original.bundle,
        )
        restored = _read_from_port(current_port, station)
        result.restored_slot = restored.slot
        result.restored = restored.bundle == original.bundle
        result.recovery_required = not result.restored
        restore_pass = (
            result.restored
            and restored.stored_crc32 == original.stored_crc32
            and restored.slot != test_snapshot.slot
            and restore_writer.target_slot == restored.slot
        )
        result.steps.append(
            ActiveStepResult(
                name="Restore original bundle",
                status=TestStatus.PASS if restore_pass else TestStatus.FAIL,
                duration_ms=round((time.monotonic() - step_started) * 1000),
                detail=(
                    f"test slot={test_snapshot.slot}, restored slot={restored.slot}, "
                    f"crc=0x{restored.stored_crc32:08X}"
                ),
                checks=[
                    _check("original_bundle_restored", "exact match", "match" if result.restored else "mismatch", result.restored),
                    _check("restore_slot_switch", f"not {test_snapshot.slot}", restored.slot, restored.slot != test_snapshot.slot),
                ],
            )
        )
        result.result = TestStatus.PASS if restore_pass else TestStatus.FAIL
    except Exception as exc:
        result.error = f"{type(exc).__name__}: {exc}"
        result.result = TestStatus.FAIL
        result.recovery_required = test_active_possible

        if original is not None and test_active_possible:
            step_started = time.monotonic()
            try:
                current_port = wait_for_lcp_reconnect(
                    current_port,
                    identity,
                    baudrate=station.serial_baudrate,
                    serial_timeout=station.serial_timeout_seconds,
                    total_timeout=20.0,
                )
                current = _read_from_port(current_port, station)
                if current.bundle != original.bundle:
                    current_port, _ = _write_and_reconnect(
                        current_port,
                        identity,
                        station,
                        original.bundle,
                    )
                    current = _read_from_port(current_port, station)
                result.restored = current.bundle == original.bundle
                result.restored_slot = current.slot
                result.recovery_required = not result.restored
                result.steps.append(
                    ActiveStepResult(
                        name="Emergency automatic restore",
                        status=TestStatus.PASS if result.restored else TestStatus.FAIL,
                        duration_ms=round((time.monotonic() - step_started) * 1000),
                        detail=(
                            "original bundle recovered after test error"
                            if result.restored
                            else "automatic recovery failed; use backup_file"
                        ),
                    )
                )
            except Exception as recovery_exc:
                result.steps.append(
                    ActiveStepResult(
                        name="Emergency automatic restore",
                        status=TestStatus.FAIL,
                        duration_ms=round((time.monotonic() - step_started) * 1000),
                        detail=f"{type(recovery_exc).__name__}: {recovery_exc}; use backup_file",
                    )
                )
                result.recovery_required = True
    finally:
        result.reconnected_port = current_port
        result.duration_ms = round((time.monotonic() - started) * 1000)

    return result
