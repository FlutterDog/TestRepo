import struct

import pytest

from lorentz_test.protocols.modbus_tcp_client import (
    ModbusTcpProtocolError,
    read_holding_registers,
)


class FakeSocket:
    response = b""
    instances: list["FakeSocket"] = []

    def __init__(self, *_: object) -> None:
        self.buffer = bytearray(self.response)
        self.sent = b""
        self.bound = None
        self.connected = None
        self.closed = False
        self.instances.append(self)

    def settimeout(self, _: float) -> None:
        pass

    def bind(self, address: tuple[str, int]) -> None:
        self.bound = address

    def connect(self, address: tuple[str, int]) -> None:
        self.connected = address

    def sendall(self, data: bytes) -> None:
        self.sent += data

    def recv(self, size: int) -> bytes:
        chunk = bytes(self.buffer[:size])
        del self.buffer[:size]
        return chunk

    def close(self) -> None:
        self.closed = True


def test_read_holding_registers_validates_mbap_and_fc03() -> None:
    registers = list(range(12))
    pdu = bytes((0x03, 24)) + struct.pack(">12H", *registers)
    FakeSocket.response = struct.pack(">HHHB", 0x7101, 0, len(pdu) + 1, 1) + pdu
    FakeSocket.instances.clear()

    result = read_holding_registers(
        "192.168.1.16",
        start_address=0,
        register_count=12,
        source_ip="192.168.1.100",
        transaction_id=0x7101,
        socket_factory=FakeSocket,
    )

    assert result.registers == registers
    instance = FakeSocket.instances[0]
    assert instance.bound == ("192.168.1.100", 0)
    assert instance.connected == ("192.168.1.16", 502)
    assert instance.sent.endswith(struct.pack(">BHH", 0x03, 0, 12))
    assert instance.closed is True


def test_read_holding_registers_rejects_wrong_transaction_id() -> None:
    pdu = bytes((0x03, 2)) + struct.pack(">H", 7)
    FakeSocket.response = struct.pack(">HHHB", 2, 0, len(pdu) + 1, 1) + pdu

    with pytest.raises(ModbusTcpProtocolError, match="transaction id"):
        read_holding_registers(
            "192.168.1.16",
            start_address=0,
            register_count=1,
            transaction_id=1,
            socket_factory=FakeSocket,
        )
