#!/usr/bin/env python3
"""Lorentz LCP2116 USB configuration reference tool.

This utility proves the complete configuration workflow independently of an
SD card:

* import the legacy canonical TXT set;
* build a normalized schema-v1 configuration model;
* validate and write the model through USB CDC;
* read the active bundle directly from internal Flash;
* export the model to JSON or back to canonical TXT files.

JSON is only a convenient test/interchange representation for this utility.
The future Studio may keep its own project model and call the same pack/unpack
functions without creating TXT or JSON files.

Requires Python 3.9+ and pyserial:

    python -m pip install pyserial
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import pathlib
import struct
import sys
import time
import zlib
from typing import Any, Iterable, Sequence

try:
    import serial
    from serial import Serial
except ImportError as exc:  # pragma: no cover - user environment check
    raise SystemExit("pyserial is required: python -m pip install pyserial") from exc


PROTOCOL_VERSION = 1
SCHEMA_VERSION = 1
BUNDLE_SIZE = 104
FRAME_HEADER_SIZE = 24
MAX_PAYLOAD_SIZE = 160
FRAME_MAGIC = b"\x00LCP"

COMMAND_HELLO = 1
COMMAND_GET_CONFIG = 2
COMMAND_VALIDATE_CONFIG = 3
COMMAND_PUT_CONFIG = 4
COMMAND_GET_STATUS = 5
COMMAND_REBOOT = 6
COMMAND_EXIT = 7

STATUS_OK = 0
STATUS_ACCEPTED = 1
STATUS_BUSY = 2
STATUS_NO_CHANGE = 3
STATUS_INVALID_CRC = 4
STATUS_UNSUPPORTED_VERSION = 5
STATUS_UNSUPPORTED_COMMAND = 6
STATUS_INVALID_LENGTH = 7
STATUS_INVALID_SCHEMA = 8
STATUS_INVALID_CONFIG = 9
STATUS_FLASH_ERROR = 10
STATUS_VERIFY_ERROR = 11
STATUS_REBOOT_REQUIRED = 12
STATUS_INTERNAL_ERROR = 13

WRITER_IDLE = 0
WRITER_WRITE_RECORD = 1
WRITER_WRITE_COMMIT = 2
WRITER_VERIFY = 3
WRITER_COMPLETE = 4
WRITER_ERROR = 5

STATUS_TEXT = {
    STATUS_OK: "ok",
    STATUS_ACCEPTED: "accepted",
    STATUS_BUSY: "busy",
    STATUS_NO_CHANGE: "no change",
    STATUS_INVALID_CRC: "invalid CRC",
    STATUS_UNSUPPORTED_VERSION: "unsupported protocol version",
    STATUS_UNSUPPORTED_COMMAND: "unsupported command",
    STATUS_INVALID_LENGTH: "invalid length",
    STATUS_INVALID_SCHEMA: "invalid schema",
    STATUS_INVALID_CONFIG: "invalid configuration",
    STATUS_FLASH_ERROR: "Flash error",
    STATUS_VERIFY_ERROR: "verify error",
    STATUS_REBOOT_REQUIRED: "reboot required",
    STATUS_INTERNAL_ERROR: "internal error",
}

LEGACY_FILES = (
    "baud.TXT",
    "Parity.TXT",
    "MAC.txt",
    "IP.txt",
    "SUBNET.txt",
    "GATE.txt",
    "MAC2.txt",
    "IP2.txt",
    "SUBNET2.txt",
    "GATE2.txt",
    "X2X.TXT",
    "EIA.TXT",
)


class ConfigError(ValueError):
    """Invalid configuration or legacy file set."""


class ProtocolError(RuntimeError):
    """USB protocol or device-side operation failed."""


@dataclasses.dataclass
class EthernetConfig:
    mac: list[int]
    ip: list[int]
    subnet: list[int]
    gateway: list[int]

    @classmethod
    def from_dict(cls, value: dict[str, Any]) -> "EthernetConfig":
        return cls(
            mac=[int(item) for item in value["mac"]],
            ip=[int(item) for item in value["ip"]],
            subnet=[int(item) for item in value["subnet"]],
            gateway=[int(item) for item in value["gateway"]],
        )


@dataclasses.dataclass
class LcpConfig:
    field_baudrate: list[int]
    field_parity: list[int]
    ethernet: list[EthernetConfig]
    x2x_modules: list[int]
    eia: list[int]
    schema_version: int = SCHEMA_VERSION

    @classmethod
    def defaults(cls) -> "LcpConfig":
        return cls(
            field_baudrate=[9600, 9600, 9600, 9600],
            field_parity=[0, 0, 0, 0],
            ethernet=[
                EthernetConfig(
                    mac=[2, 76, 67, 80, 0, 1],
                    ip=[192, 168, 1, 1],
                    subnet=[255, 255, 255, 0],
                    gateway=[192, 168, 1, 254],
                ),
                EthernetConfig(
                    mac=[2, 76, 67, 80, 0, 2],
                    ip=[192, 168, 1, 2],
                    subnet=[255, 255, 255, 0],
                    gateway=[192, 168, 1, 254],
                ),
            ],
            x2x_modules=[],
            eia=[3, 4, 5, 6],
        )

    @classmethod
    def from_dict(cls, value: dict[str, Any]) -> "LcpConfig":
        return cls(
            schema_version=int(value.get("schema_version", SCHEMA_VERSION)),
            field_baudrate=[int(item) for item in value["field_baudrate"]],
            field_parity=[int(item) for item in value["field_parity"]],
            ethernet=[EthernetConfig.from_dict(item) for item in value["ethernet"]],
            x2x_modules=[int(item) for item in value["x2x_modules"]],
            eia=[int(item) for item in value["eia"]],
        )

    def to_dict(self) -> dict[str, Any]:
        return dataclasses.asdict(self)

    def validate(self) -> None:
        if self.schema_version != SCHEMA_VERSION:
            raise ConfigError(f"unsupported schema version: {self.schema_version}")

        _require_length("field_baudrate", self.field_baudrate, 4)
        _require_length("field_parity", self.field_parity, 4)
        _require_length("ethernet", self.ethernet, 2)
        _require_length("eia", self.eia, 4)

        for index, baudrate in enumerate(self.field_baudrate, start=1):
            if not 300 <= baudrate <= 1_000_000:
                raise ConfigError(f"S{index} baudrate outside 300..1000000")

        for index, parity in enumerate(self.field_parity, start=1):
            if parity not in (0, 1, 2):
                raise ConfigError(f"S{index} parity must be 0, 1 or 2")

        for index, interface in enumerate(self.ethernet, start=1):
            _validate_octets(f"ETH{index}.mac", interface.mac, 6)
            _validate_octets(f"ETH{index}.ip", interface.ip, 4)
            _validate_octets(f"ETH{index}.subnet", interface.subnet, 4)
            _validate_octets(f"ETH{index}.gateway", interface.gateway, 4)
            _validate_mac(f"ETH{index}.mac", interface.mac)
            _validate_ip(f"ETH{index}.ip", interface.ip)
            _validate_subnet(f"ETH{index}.subnet", interface.subnet)

        if len(self.x2x_modules) > 6:
            raise ConfigError("X2X module count exceeds 6")

        for module_id in self.x2x_modules:
            if not 1 <= module_id <= 255:
                raise ConfigError(f"invalid X2X module ID: {module_id}")

        for value in self.eia:
            if not 0 <= value <= 255:
                raise ConfigError(f"EIA value outside 0..255: {value}")

    def pack(self) -> bytes:
        self.validate()
        output = bytearray()
        output.extend(struct.pack("<4I", *self.field_baudrate))
        output.extend(bytes(self.field_parity))

        for interface in self.ethernet:
            output.extend(bytes(interface.mac))
        for interface in self.ethernet:
            output.extend(bytes(interface.ip))
        for interface in self.ethernet:
            output.extend(bytes(interface.subnet))
        for interface in self.ethernet:
            output.extend(bytes(interface.gateway))

        output.append(len(self.x2x_modules))
        output.extend(bytes(self.x2x_modules))
        output.extend(bytes(6 - len(self.x2x_modules)))
        output.append(4)
        output.extend(struct.pack("<4h", *self.eia))
        output.extend(bytes(32))

        if len(output) != BUNDLE_SIZE:
            raise AssertionError(f"bundle size {len(output)} != {BUNDLE_SIZE}")
        return bytes(output)

    @classmethod
    def unpack(cls, payload: bytes) -> "LcpConfig":
        if len(payload) != BUNDLE_SIZE:
            raise ConfigError(f"bundle size {len(payload)} != {BUNDLE_SIZE}")

        position = 0
        field_baudrate = list(struct.unpack_from("<4I", payload, position))
        position += 16
        field_parity = list(payload[position:position + 4])
        position += 4

        macs = [list(payload[position + 6 * i:position + 6 * (i + 1)])
                for i in range(2)]
        position += 12
        ips = [list(payload[position + 4 * i:position + 4 * (i + 1)])
               for i in range(2)]
        position += 8
        subnets = [list(payload[position + 4 * i:position + 4 * (i + 1)])
                   for i in range(2)]
        position += 8
        gateways = [list(payload[position + 4 * i:position + 4 * (i + 1)])
                    for i in range(2)]
        position += 8

        x2x_count = payload[position]
        position += 1
        x2x_storage = list(payload[position:position + 6])
        position += 6
        eia_count = payload[position]
        position += 1
        eia = list(struct.unpack_from("<4h", payload, position))
        position += 8
        reserved = payload[position:position + 32]

        if x2x_count > 6:
            raise ConfigError(f"invalid stored X2X count: {x2x_count}")
        if eia_count != 4:
            raise ConfigError(f"invalid stored EIA count: {eia_count}")
        if any(reserved):
            raise ConfigError("reserved bytes are not zero")
        if any(x2x_storage[x2x_count:]):
            raise ConfigError("unused X2X slots are not zero")

        config = cls(
            field_baudrate=field_baudrate,
            field_parity=field_parity,
            ethernet=[
                EthernetConfig(macs[i], ips[i], subnets[i], gateways[i])
                for i in range(2)
            ],
            x2x_modules=x2x_storage[:x2x_count],
            eia=eia,
        )
        config.validate()
        return config


@dataclasses.dataclass
class ActiveConfigInfo:
    sequence: int
    generation: int
    crc32: int
    source: int
    slot: int
    config: LcpConfig


@dataclasses.dataclass
class WriterInfo:
    state: int
    result: int
    active_source: int
    active_slot: int
    target_slot: int
    service_busy: bool
    active_sequence: int
    target_sequence: int
    active_crc32: int
    generation: int
    flash_result: int


@dataclasses.dataclass
class Frame:
    command: int
    status: int
    flags: int
    request_id: int
    payload: bytes


class LcpUsbClient:
    def __init__(self, port: str, baudrate: int = 115200,
                 timeout: float = 2.0) -> None:
        self.serial: Serial = serial.Serial(
            port=port,
            baudrate=baudrate,
            timeout=timeout,
            write_timeout=timeout,
        )
        self._request_id = 1

    def close(self) -> None:
        if self.serial.is_open:
            try:
                self.request(COMMAND_EXIT, b"", accepted={STATUS_OK}, timeout=0.5)
            except Exception:
                pass
            self.serial.close()

    def __enter__(self) -> "LcpUsbClient":
        return self

    def __exit__(self, exc_type: Any, exc: Any, traceback: Any) -> None:
        self.close()

    def _next_request_id(self) -> int:
        result = self._request_id
        self._request_id = (self._request_id + 1) & 0xFFFFFFFF
        if self._request_id == 0:
            self._request_id = 1
        return result

    @staticmethod
    def _build_frame(command: int, request_id: int, payload: bytes) -> bytes:
        if len(payload) > MAX_PAYLOAD_SIZE:
            raise ProtocolError("payload exceeds protocol maximum")
        prefix = struct.pack(
            "<4sBBBBIII",
            FRAME_MAGIC,
            PROTOCOL_VERSION,
            command,
            0,
            0,
            request_id,
            len(payload),
            crc32(payload),
        )
        return prefix + struct.pack("<I", crc32(prefix)) + payload

    def _read_exact(self, length: int, deadline: float) -> bytes:
        output = bytearray()
        while len(output) < length:
            if time.monotonic() >= deadline:
                raise ProtocolError(
                    f"timeout while reading {length} bytes; received {len(output)}"
                )
            chunk = self.serial.read(length - len(output))
            if chunk:
                output.extend(chunk)
        return bytes(output)

    def _find_magic(self, deadline: float) -> None:
        matched = 0
        while matched < len(FRAME_MAGIC):
            if time.monotonic() >= deadline:
                raise ProtocolError("timeout waiting for LCP USB frame")
            value = self.serial.read(1)
            if not value:
                continue
            byte = value[0]
            if byte == FRAME_MAGIC[matched]:
                matched += 1
            else:
                matched = 1 if byte == FRAME_MAGIC[0] else 0

    def _read_frame(self, timeout: float) -> Frame:
        deadline = time.monotonic() + timeout
        self._find_magic(deadline)
        rest = self._read_exact(FRAME_HEADER_SIZE - 4, deadline)
        header = FRAME_MAGIC + rest
        (
            magic,
            version,
            command,
            status,
            flags,
            request_id,
            payload_length,
            payload_crc32,
            header_crc32,
        ) = struct.unpack("<4sBBBBIIII", header)

        if magic != FRAME_MAGIC:
            raise ProtocolError("invalid frame magic")
        if version != PROTOCOL_VERSION:
            raise ProtocolError(f"protocol version {version} is not supported")
        if crc32(header[:20]) != header_crc32:
            raise ProtocolError("header CRC mismatch")
        if payload_length > MAX_PAYLOAD_SIZE:
            raise ProtocolError(f"response payload too large: {payload_length}")

        payload = self._read_exact(payload_length, deadline)
        if crc32(payload) != payload_crc32:
            raise ProtocolError("payload CRC mismatch")
        return Frame(command, status, flags, request_id, payload)

    def request(self, command: int, payload: bytes = b"", *,
                accepted: set[int] | None = None,
                timeout: float = 2.0) -> Frame:
        request_id = self._next_request_id()
        self.serial.write(self._build_frame(command, request_id, payload))
        self.serial.flush()

        while True:
            frame = self._read_frame(timeout)
            if frame.request_id != request_id:
                continue
            if frame.command != command:
                raise ProtocolError(
                    f"response command {frame.command} != request {command}"
                )
            if accepted is not None and frame.status not in accepted:
                raise ProtocolError(
                    f"device returned {STATUS_TEXT.get(frame.status, frame.status)}"
                )
            return frame

    def hello(self) -> dict[str, int]:
        frame = self.request(COMMAND_HELLO, accepted={STATUS_OK})
        if len(frame.payload) != 16:
            raise ProtocolError("invalid HELLO response length")
        (
            protocol_version,
            schema_version,
            bundle_size,
            max_payload,
            capabilities,
            storage_version,
            header_size,
        ) = struct.unpack("<HHHHIHH", frame.payload)
        return {
            "protocol_version": protocol_version,
            "schema_version": schema_version,
            "bundle_size": bundle_size,
            "max_payload": max_payload,
            "capabilities": capabilities,
            "storage_version": storage_version,
            "header_size": header_size,
        }

    def read_config(self) -> ActiveConfigInfo:
        frame = self.request(COMMAND_GET_CONFIG, accepted={STATUS_OK})
        if len(frame.payload) != 20 + BUNDLE_SIZE:
            raise ProtocolError("invalid GET_CONFIG response length")
        schema_version, bundle_size = struct.unpack_from("<HH", frame.payload, 0)
        sequence, generation, stored_crc = struct.unpack_from(
            "<III", frame.payload, 4
        )
        source = frame.payload[16]
        slot = frame.payload[17]
        bundle = frame.payload[20:]
        if schema_version != SCHEMA_VERSION or bundle_size != BUNDLE_SIZE:
            raise ProtocolError("unsupported configuration schema")
        if crc32(bundle) != stored_crc:
            raise ProtocolError("active configuration CRC mismatch")
        return ActiveConfigInfo(
            sequence=sequence,
            generation=generation,
            crc32=stored_crc,
            source=source,
            slot=slot,
            config=LcpConfig.unpack(bundle),
        )

    def validate_config(self, config: LcpConfig) -> None:
        bundle = config.pack()
        payload = struct.pack("<HHI", SCHEMA_VERSION, len(bundle), crc32(bundle)) + bundle
        self.request(COMMAND_VALIDATE_CONFIG, payload, accepted={STATUS_OK})

    def write_config(self, config: LcpConfig, *, reboot: bool = True,
                     wait_timeout: float = 5.0) -> int:
        bundle = config.pack()
        payload = struct.pack("<HHI", SCHEMA_VERSION, len(bundle), crc32(bundle)) + bundle
        self.validate_config(config)
        response = self.request(
            COMMAND_PUT_CONFIG,
            payload,
            accepted={STATUS_ACCEPTED, STATUS_NO_CHANGE},
        )
        if response.status == STATUS_NO_CHANGE:
            return STATUS_NO_CHANGE

        deadline = time.monotonic() + wait_timeout
        while True:
            writer = self.writer_status()
            if writer.state == WRITER_COMPLETE:
                if writer.result != STATUS_REBOOT_REQUIRED:
                    raise ProtocolError(
                        f"unexpected completed result: {writer.result}"
                    )
                break
            if writer.state == WRITER_ERROR:
                raise ProtocolError(
                    "configuration write failed: "
                    f"{STATUS_TEXT.get(writer.result, writer.result)}, "
                    f"Flash result={writer.flash_result}"
                )
            if time.monotonic() >= deadline:
                raise ProtocolError("timeout waiting for Flash commit")
            time.sleep(0.05)

        if reboot:
            self.request(COMMAND_REBOOT, accepted={STATUS_OK}, timeout=1.0)
        return STATUS_REBOOT_REQUIRED

    def writer_status(self) -> WriterInfo:
        frame = self.request(COMMAND_GET_STATUS, accepted={STATUS_OK})
        if len(frame.payload) != 28:
            raise ProtocolError("invalid GET_STATUS response length")
        return WriterInfo(
            state=frame.payload[0],
            result=frame.payload[1],
            active_source=frame.payload[2],
            active_slot=frame.payload[3],
            target_slot=frame.payload[4],
            service_busy=bool(frame.payload[5]),
            active_sequence=struct.unpack_from("<I", frame.payload, 8)[0],
            target_sequence=struct.unpack_from("<I", frame.payload, 12)[0],
            active_crc32=struct.unpack_from("<I", frame.payload, 16)[0],
            generation=struct.unpack_from("<I", frame.payload, 20)[0],
            flash_result=frame.payload[24],
        )


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def _require_length(name: str, values: Sequence[Any], length: int) -> None:
    if len(values) != length:
        raise ConfigError(f"{name} requires {length} values, got {len(values)}")


def _validate_octets(name: str, values: Sequence[int], length: int) -> None:
    _require_length(name, values, length)
    for value in values:
        if not 0 <= value <= 255:
            raise ConfigError(f"{name} contains value outside 0..255: {value}")


def _validate_mac(name: str, values: Sequence[int]) -> None:
    if not any(values):
        raise ConfigError(f"{name} must not be all zero")
    if values[0] & 0x01:
        raise ConfigError(f"{name} must be an individual, not multicast, address")


def _validate_ip(name: str, values: Sequence[int]) -> None:
    if values == [0, 0, 0, 0]:
        raise ConfigError(f"{name} must not be 0.0.0.0")
    if 224 <= values[0] <= 239:
        raise ConfigError(f"{name} must not be multicast")
    if values == [255, 255, 255, 255]:
        raise ConfigError(f"{name} must not be limited broadcast")


def _validate_subnet(name: str, values: Sequence[int]) -> None:
    mask = int.from_bytes(bytes(values), "big")
    if mask == 0:
        raise ConfigError(f"{name} must not be zero")
    inverted = (~mask) & 0xFFFFFFFF
    if inverted & (inverted + 1):
        raise ConfigError(f"{name} is not a contiguous subnet mask")


def _strip_line(line: str) -> str:
    line = line.lstrip("\ufeff")
    comment_positions = [position for marker in ("//", "#")
                         if (position := line.find(marker)) >= 0]
    if comment_positions:
        line = line[:min(comment_positions)]
    return line.strip()


def read_numeric_file(path: pathlib.Path, *, allow_zero_count: bool = False) -> list[int]:
    if not path.is_file():
        raise ConfigError(f"required file not found: {path.name}")
    logical_lines = []
    for physical_line, raw_line in enumerate(
            path.read_text(encoding="utf-8-sig").splitlines(), start=1):
        line = _strip_line(raw_line)
        if not line:
            continue
        if line.lower() == "fin":
            break
        try:
            logical_lines.append(int(line, 10))
        except ValueError as exc:
            raise ConfigError(
                f"{path.name}:{physical_line}: invalid integer {line!r}"
            ) from exc

    if not logical_lines:
        raise ConfigError(f"{path.name}: empty file")
    count = logical_lines[0]
    if count < 0 or (count == 0 and not allow_zero_count):
        raise ConfigError(f"{path.name}: invalid count {count}")
    values = logical_lines[1:]
    if len(values) < count:
        raise ConfigError(
            f"{path.name}: incomplete file, expected {count} values, got {len(values)}"
        )
    if len(values) > count:
        raise ConfigError(
            f"{path.name}: extra data, expected {count} values, got {len(values)}"
        )
    return values


def load_legacy_txt(directory: pathlib.Path) -> LcpConfig:
    directory = directory.resolve()
    baud = read_numeric_file(directory / "baud.TXT")
    parity = read_numeric_file(directory / "Parity.TXT")
    eth1 = EthernetConfig(
        mac=read_numeric_file(directory / "MAC.txt"),
        ip=read_numeric_file(directory / "IP.txt"),
        subnet=read_numeric_file(directory / "SUBNET.txt"),
        gateway=read_numeric_file(directory / "GATE.txt"),
    )
    eth2 = EthernetConfig(
        mac=read_numeric_file(directory / "MAC2.txt"),
        ip=read_numeric_file(directory / "IP2.txt"),
        subnet=read_numeric_file(directory / "SUBNET2.txt"),
        gateway=read_numeric_file(directory / "GATE2.txt"),
    )
    x2x = read_numeric_file(directory / "X2X.TXT", allow_zero_count=True)
    eia = read_numeric_file(directory / "EIA.TXT")
    config = LcpConfig(baud, parity, [eth1, eth2], x2x, eia)
    config.validate()
    return config


def _canonical_numeric(values: Iterable[int]) -> str:
    values = list(values)
    return "\n".join([str(len(values)), *(str(value) for value in values), "fin", ""])


def export_legacy_txt(config: LcpConfig, directory: pathlib.Path) -> None:
    config.validate()
    directory.mkdir(parents=True, exist_ok=True)
    contents = {
        "baud.TXT": config.field_baudrate,
        "Parity.TXT": config.field_parity,
        "MAC.txt": config.ethernet[0].mac,
        "IP.txt": config.ethernet[0].ip,
        "SUBNET.txt": config.ethernet[0].subnet,
        "GATE.txt": config.ethernet[0].gateway,
        "MAC2.txt": config.ethernet[1].mac,
        "IP2.txt": config.ethernet[1].ip,
        "SUBNET2.txt": config.ethernet[1].subnet,
        "GATE2.txt": config.ethernet[1].gateway,
        "X2X.TXT": config.x2x_modules,
        "EIA.TXT": config.eia,
    }
    for name, values in contents.items():
        (directory / name).write_text(_canonical_numeric(values), encoding="utf-8")


def load_json(path: pathlib.Path) -> LcpConfig:
    return LcpConfig.from_dict(json.loads(path.read_text(encoding="utf-8")))


def save_json(config: LcpConfig, path: pathlib.Path) -> None:
    config.validate()
    path.write_text(
        json.dumps(config.to_dict(), ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def print_active(info: ActiveConfigInfo) -> None:
    print(f"source={info.source} slot={info.slot} sequence={info.sequence}")
    print(f"generation={info.generation} crc32=0x{info.crc32:08X}")
    print(json.dumps(info.config.to_dict(), ensure_ascii=False, indent=2))


def command_info(args: argparse.Namespace) -> None:
    with LcpUsbClient(args.port, timeout=args.timeout) as client:
        print(json.dumps(client.hello(), indent=2))
        print(dataclasses.asdict(client.writer_status()))


def command_read(args: argparse.Namespace) -> None:
    with LcpUsbClient(args.port, timeout=args.timeout) as client:
        client.hello()
        info = client.read_config()
    print_active(info)
    if args.output:
        save_json(info.config, args.output)


def command_write(args: argparse.Namespace) -> None:
    config = load_json(args.input)
    with LcpUsbClient(args.port, timeout=args.timeout) as client:
        client.hello()
        result = client.write_config(config, reboot=not args.no_reboot)
    print(STATUS_TEXT[result])


def command_write_txt(args: argparse.Namespace) -> None:
    config = load_legacy_txt(args.directory)
    with LcpUsbClient(args.port, timeout=args.timeout) as client:
        client.hello()
        result = client.write_config(config, reboot=not args.no_reboot)
    print(STATUS_TEXT[result])


def command_import_txt(args: argparse.Namespace) -> None:
    config = load_legacy_txt(args.directory)
    save_json(config, args.output)
    print(f"saved {args.output}")


def command_export_txt(args: argparse.Namespace) -> None:
    config = load_json(args.input)
    export_legacy_txt(config, args.directory)
    print(f"exported canonical TXT files to {args.directory}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    def add_usb_options(command: argparse.ArgumentParser) -> None:
        command.add_argument("--port", required=True, help="COM port, e.g. COM8")
        command.add_argument("--timeout", type=float, default=2.0)

    info = subparsers.add_parser("info", help="read protocol and writer status")
    add_usb_options(info)
    info.set_defaults(handler=command_info)

    read = subparsers.add_parser("read", help="read active configuration from Flash")
    add_usb_options(read)
    read.add_argument("--output", type=pathlib.Path)
    read.set_defaults(handler=command_read)

    write = subparsers.add_parser("write", help="write JSON model to Flash")
    add_usb_options(write)
    write.add_argument("--input", type=pathlib.Path, required=True)
    write.add_argument("--no-reboot", action="store_true")
    write.set_defaults(handler=command_write)

    write_txt = subparsers.add_parser(
        "write-txt", help="parse legacy TXT directory and write it to Flash"
    )
    add_usb_options(write_txt)
    write_txt.add_argument("--directory", type=pathlib.Path, required=True)
    write_txt.add_argument("--no-reboot", action="store_true")
    write_txt.set_defaults(handler=command_write_txt)

    import_txt = subparsers.add_parser(
        "import-txt", help="convert legacy TXT directory to JSON model"
    )
    import_txt.add_argument("--directory", type=pathlib.Path, required=True)
    import_txt.add_argument("--output", type=pathlib.Path, required=True)
    import_txt.set_defaults(handler=command_import_txt)

    export_txt = subparsers.add_parser(
        "export-txt", help="export JSON model to canonical legacy TXT files"
    )
    export_txt.add_argument("--input", type=pathlib.Path, required=True)
    export_txt.add_argument("--directory", type=pathlib.Path, required=True)
    export_txt.set_defaults(handler=command_export_txt)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        args.handler(args)
        return 0
    except (ConfigError, ProtocolError, serial.SerialException, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
