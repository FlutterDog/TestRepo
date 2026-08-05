#!/usr/bin/env python3
"""Verify a Lorentz Fixture TCP-to-RS485 bridge against a serial-server port.

The PC opens both ends of the physical path:

    fixture TCP endpoint <-> fixture S1 <-> RS-485 <-> serial server <-> peer endpoint

It sends deterministic binary payloads in both directions, checks exact byte
identity, exercises TCP reconnects, and writes a machine-readable JSON report.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import random
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import serial


DEFAULT_FIXTURE_ENDPOINT = "socket://192.168.1.200:2101"
DEFAULT_LENGTHS = (1, 2, 3, 7, 8, 16, 31, 64, 127)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def open_endpoint(url: str, baudrate: int, timeout: float) -> serial.SerialBase:
    endpoint = serial.serial_for_url(
        url,
        baudrate=baudrate,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=timeout,
        write_timeout=max(2.0, timeout),
    )
    endpoint.reset_input_buffer()
    endpoint.reset_output_buffer()
    return endpoint


def safe_reset(endpoint: serial.SerialBase) -> None:
    try:
        endpoint.reset_input_buffer()
    except (serial.SerialException, OSError):
        pass
    try:
        endpoint.reset_output_buffer()
    except (serial.SerialException, OSError):
        pass


def drain(endpoint: serial.SerialBase, quiet_s: float = 0.05) -> bytes:
    data = bytearray()
    quiet_deadline = time.monotonic() + quiet_s
    while time.monotonic() < quiet_deadline:
        waiting = int(getattr(endpoint, "in_waiting", 0) or 0)
        chunk = endpoint.read(waiting if waiting > 0 else 1)
        if chunk:
            data.extend(chunk)
            quiet_deadline = time.monotonic() + quiet_s
        else:
            time.sleep(0.002)
    return bytes(data)


def read_exact(endpoint: serial.SerialBase, length: int, deadline_s: float) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + deadline_s
    while len(data) < length and time.monotonic() < deadline:
        chunk = endpoint.read(length - len(data))
        if chunk:
            data.extend(chunk)
        else:
            time.sleep(0.001)
    return bytes(data)


def payload_digest(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()[:16]


def make_payloads(seed: int, rounds: int, lengths: tuple[int, ...]) -> list[bytes]:
    rng = random.Random(seed)
    payloads: list[bytes] = [
        b"\x00",
        b"\x55\xAA\x00\xFF",
        bytes(range(16)),
        bytes(reversed(range(32))),
    ]
    for _ in range(rounds):
        for length in lengths:
            payloads.append(bytes(rng.randrange(0, 256) for _ in range(length)))
    return payloads


def transfer_once(
    source: serial.SerialBase,
    destination: serial.SerialBase,
    payload: bytes,
    direction: str,
    timeout_s: float,
    inter_frame_s: float,
) -> dict[str, Any]:
    safe_reset(source)
    safe_reset(destination)
    time.sleep(inter_frame_s)

    started = time.monotonic()
    source.write(payload)
    source.flush()
    received = read_exact(destination, len(payload), timeout_s)
    latency_ms = round((time.monotonic() - started) * 1000.0, 3)
    extra = drain(destination)

    passed = received == payload and not extra
    result: dict[str, Any] = {
        "direction": direction,
        "length": len(payload),
        "sha256_16": payload_digest(payload),
        "latency_ms": latency_ms,
        "received_length": len(received),
        "extra_length": len(extra),
        "status": "PASS" if passed else "FAIL",
    }
    if received != payload:
        result["expected_hex"] = payload.hex()
        result["received_hex"] = received.hex()
    if extra:
        result["extra_hex"] = extra.hex()
    return result


def query_fixture_usb(port: str, command: str = "status") -> str:
    with serial.Serial(
        port=port,
        baudrate=115200,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.05,
        write_timeout=1.0,
    ) as console:
        time.sleep(0.35)
        safe_reset(console)
        console.write((command + "\r\n").encode("ascii"))
        console.flush()

        response = bytearray()
        deadline = time.monotonic() + 3.0
        quiet_deadline = time.monotonic() + 0.4
        while time.monotonic() < deadline:
            chunk = console.read(max(1, int(console.in_waiting or 0)))
            if chunk:
                response.extend(chunk)
                quiet_deadline = time.monotonic() + 0.4
            elif response and time.monotonic() >= quiet_deadline:
                break
            else:
                time.sleep(0.01)
        return response.decode("utf-8", errors="replace").strip()


def parse_lengths(value: str) -> tuple[int, ...]:
    lengths = tuple(int(item.strip()) for item in value.split(",") if item.strip())
    if not lengths or any(length < 1 or length > 512 for length in lengths):
        raise argparse.ArgumentTypeError("lengths must contain values from 1 to 512")
    return lengths


def default_output_path() -> Path:
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return Path(f"fixture_bridge_verify_{stamp}.json")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Verify fixture TCP<->S1 transport using a serial-server peer."
    )
    parser.add_argument("--fixture", default=DEFAULT_FIXTURE_ENDPOINT)
    parser.add_argument(
        "--peer",
        required=True,
        help="Peer endpoint, for example COM4 or socket://192.168.1.50:4001",
    )
    parser.add_argument("--fixture-usb", help="Optional fixture management COM port")
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--rounds", type=int, default=2)
    parser.add_argument("--lengths", type=parse_lengths, default=DEFAULT_LENGTHS)
    parser.add_argument("--seed", type=int, default=2116)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--serial-timeout", type=float, default=0.05)
    parser.add_argument("--inter-frame-ms", type=float, default=30.0)
    parser.add_argument("--reconnects", type=int, default=3)
    parser.add_argument("--output", type=Path, default=None)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    output = args.output or default_output_path()
    report: dict[str, Any] = {
        "test_id": "fixture_bridge_local_loopback",
        "started_at": utc_now(),
        "fixture_endpoint": args.fixture,
        "peer_endpoint": args.peer,
        "fixture_usb": args.fixture_usb,
        "serial_format": f"{args.baud} 8N1",
        "seed": args.seed,
        "rounds": args.rounds,
        "lengths": list(args.lengths),
        "reconnect_count_requested": args.reconnects,
        "transfers": [],
        "reconnects": [],
        "errors": [],
    }

    fixture: serial.SerialBase | None = None
    peer: serial.SerialBase | None = None
    try:
        if args.fixture_usb:
            report["fixture_status_before"] = query_fixture_usb(args.fixture_usb)

        fixture = open_endpoint(args.fixture, args.baud, args.serial_timeout)
        peer = open_endpoint(args.peer, args.baud, args.serial_timeout)
        time.sleep(0.25)
        safe_reset(fixture)
        safe_reset(peer)

        payloads = make_payloads(args.seed, args.rounds, args.lengths)
        inter_frame_s = max(0.0, args.inter_frame_ms / 1000.0)
        for payload in payloads:
            report["transfers"].append(
                transfer_once(
                    fixture,
                    peer,
                    payload,
                    "fixture_to_peer",
                    args.timeout,
                    inter_frame_s,
                )
            )
            report["transfers"].append(
                transfer_once(
                    peer,
                    fixture,
                    payload,
                    "peer_to_fixture",
                    args.timeout,
                    inter_frame_s,
                )
            )

        reconnect_payload = b"LCP2116-FIXTURE-RECONNECT"
        for index in range(1, args.reconnects + 1):
            fixture.close()
            time.sleep(0.35)
            fixture = open_endpoint(args.fixture, args.baud, args.serial_timeout)
            time.sleep(0.25)

            forward = transfer_once(
                fixture,
                peer,
                reconnect_payload + bytes([index]),
                "fixture_to_peer",
                args.timeout,
                inter_frame_s,
            )
            reverse = transfer_once(
                peer,
                fixture,
                reconnect_payload[::-1] + bytes([index]),
                "peer_to_fixture",
                args.timeout,
                inter_frame_s,
            )
            report["reconnects"].append(
                {
                    "index": index,
                    "status": (
                        "PASS"
                        if forward["status"] == "PASS" and reverse["status"] == "PASS"
                        else "FAIL"
                    ),
                    "forward": forward,
                    "reverse": reverse,
                }
            )

        if args.fixture_usb:
            report["fixture_status_after"] = query_fixture_usb(args.fixture_usb)

    except (serial.SerialException, OSError, ValueError) as exc:
        report["errors"].append(f"{type(exc).__name__}: {exc}")
    finally:
        if fixture is not None and fixture.is_open:
            fixture.close()
        if peer is not None and peer.is_open:
            peer.close()

    transfer_failures = [
        item for item in report["transfers"] if item.get("status") != "PASS"
    ]
    reconnect_failures = [
        item for item in report["reconnects"] if item.get("status") != "PASS"
    ]
    report["summary"] = {
        "transfer_count": len(report["transfers"]),
        "transfer_passed": len(report["transfers"]) - len(transfer_failures),
        "transfer_failed": len(transfer_failures),
        "reconnect_count": len(report["reconnects"]),
        "reconnect_failed": len(reconnect_failures),
    }
    report["status"] = (
        "PASS"
        if not report["errors"] and not transfer_failures and not reconnect_failures
        else "FAIL"
    )
    report["finished_at"] = utc_now()

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    print(f"Fixture bridge verification: {report['status']}")
    print(
        "Transfers: "
        f"{report['summary']['transfer_passed']}/{report['summary']['transfer_count']} PASS"
    )
    print(
        "Reconnects: "
        f"{report['summary']['reconnect_count'] - report['summary']['reconnect_failed']}"
        f"/{report['summary']['reconnect_count']} PASS"
    )
    if report["errors"]:
        for error in report["errors"]:
            print(f"ERROR: {error}", file=sys.stderr)
    print(f"Report: {output.resolve()}")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
