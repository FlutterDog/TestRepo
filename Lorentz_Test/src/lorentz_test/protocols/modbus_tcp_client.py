"""Minimal strict Modbus TCP client used by the LCP production test."""

from __future__ import annotations

import socket
import struct
from dataclasses import dataclass


class ModbusTcpError(RuntimeError):
    """Transport or protocol failure during one Modbus TCP transaction."""


@dataclass(frozen=True)
class ModbusTcpReadResult:
    transaction_id: int
    unit_id: int
    registers: list[int]


def _recv_exact(sock: socket.socket, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ModbusTcpError(f"connection closed after {len(data)} of {size} bytes")
        data.extend(chunk)
    return bytes(data)


def read_holding_registers(
    host: str,
    *,
    start_address: int,
    register_count: int,
    source_ip: str | None = None,
    port: int = 502,
    unit_id: int = 1,
    transaction_id: int = 1,
    timeout: float = 2.0,
    socket_factory: type[socket.socket] = socket.socket,
) -> ModbusTcpReadResult:
    """Read holding registers with one validated FC03 transaction."""
    if not 0 <= start_address <= 0xFFFF:
        raise ValueError("start_address must fit uint16")
    if not 1 <= register_count <= 125:
        raise ValueError("register_count must be in range 1..125")
    if not 0 <= unit_id <= 0xFF:
        raise ValueError("unit_id must fit uint8")
    if not 0 <= transaction_id <= 0xFFFF:
        raise ValueError("transaction_id must fit uint16")

    pdu = struct.pack(">BHH", 0x03, start_address, register_count)
    request = struct.pack(">HHHB", transaction_id, 0, len(pdu) + 1, unit_id) + pdu

    sock = socket_factory(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.settimeout(timeout)
        if source_ip:
            sock.bind((source_ip, 0))
        sock.connect((host, port))
        sock.sendall(request)

        mbap = _recv_exact(sock, 7)
        rx_transaction, protocol_id, length, rx_unit = struct.unpack(">HHHB", mbap)
        if rx_transaction != transaction_id:
            raise ModbusTcpError(
                f"transaction id {rx_transaction} != expected {transaction_id}"
            )
        if protocol_id != 0:
            raise ModbusTcpError(f"protocol id {protocol_id} != 0")
        if rx_unit != unit_id:
            raise ModbusTcpError(f"unit id {rx_unit} != expected {unit_id}")
        if length < 3 or length > 254:
            raise ModbusTcpError(f"invalid MBAP length {length}")

        response_pdu = _recv_exact(sock, length - 1)
        function = response_pdu[0]
        if function == (0x03 | 0x80):
            if len(response_pdu) != 2:
                raise ModbusTcpError("malformed Modbus exception response")
            raise ModbusTcpError(f"device returned Modbus exception {response_pdu[1]}")
        if function != 0x03:
            raise ModbusTcpError(f"function 0x{function:02X} != 0x03")
        if len(response_pdu) < 2:
            raise ModbusTcpError("truncated FC03 response")

        byte_count = response_pdu[1]
        expected_bytes = register_count * 2
        if byte_count != expected_bytes:
            raise ModbusTcpError(
                f"byte count {byte_count} != expected {expected_bytes}"
            )
        if len(response_pdu) != 2 + byte_count:
            raise ModbusTcpError(
                f"PDU length {len(response_pdu)} != expected {2 + byte_count}"
            )

        registers = list(struct.unpack(f">{register_count}H", response_pdu[2:]))
        return ModbusTcpReadResult(transaction_id, unit_id, registers)
    except (OSError, socket.timeout) as exc:
        raise ModbusTcpError(str(exc)) from exc
    finally:
        try:
            sock.close()
        except OSError:
            pass
