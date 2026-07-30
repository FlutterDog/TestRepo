import serial

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpHelloRequest, TestStatus
from lorentz_test.protocols.modbus_rtu_slave import RtuRequest, RtuSlaveStats
from lorentz_test.services.lcp_active_rs485 import run_lcp_active_rs485


FIELD_BEFORE = """
[ FIELDSENSOR S1..S4 ]
service = running
-- S1 --
serial = 9600 8N1
request_slave = 1
connection = lost, valid = no
last_result = timeout
register[0] = 0, register[1] = 0
success = 0
"""

FIELD_AFTER = """
[ FIELDSENSOR S1..S4 ]
service = running
-- S1 --
serial = 9600 8N1
request_slave = 1
connection = online, valid = yes
last_result = ok
register[0] = 4353, register[1] = 4354
success = 4
"""

RS485 = """
[ RS-485 PORTS ]
-- X2X physical port --
serial = 9600 8N1
owner = X2X master, echo = disabled, uart_errors = 0
"""

X2X_BEFORE = """
[ X2X MODULE BUS ]
-- Runtime --
module_count = 1
-- MODULE slave = 1 --
slave = 1, type_name = LCT1114_2
connection = lost
communication_error = timeout
success = 0, consecutive_failures = 255
"""

X2X_AFTER = """
[ X2X MODULE BUS ]
-- Runtime --
module_count = 1
-- MODULE slave = 1 --
slave = 1, type_name = LCT1114_2
connection = online
communication_error = ok
success = 1, consecutive_failures = 0
"""


class FakeConsole:
    def __init__(self) -> None:
        self.counts: dict[str, int] = {}
        self.closed = False

    @classmethod
    def open_port(cls, *_: object, **__: object) -> "FakeConsole":
        return cls()

    def execute(self, command: str) -> str:
        self.counts[command] = self.counts.get(command, 0) + 1
        if command == "field":
            return FIELD_BEFORE if self.counts[command] == 1 else FIELD_AFTER
        if command == "rs485":
            return RS485
        if command == "x2x":
            return X2X_BEFORE if self.counts[command] == 1 else X2X_AFTER
        raise AssertionError(command)

    def close(self) -> None:
        self.closed = True


class FakeServer:
    instances: list["FakeServer"] = []

    def __init__(self, **kwargs: object) -> None:
        self.port = str(kwargs["port"])
        self.stats = RtuSlaveStats()
        self.started = False
        self.stopped = False
        if self.port == "COM11":
            self.stats.requests = [RtuRequest(1, 0x03, 0, 2)]
            self.stats.requests_received = 4
            self.stats.responses_sent = 4
        else:
            addresses = [0, 16, 32, 48, 64, 80, 850]
            self.stats.requests = [RtuRequest(1, 0x03, address, 1 if address == 850 else 16) for address in addresses]
            self.stats.requests_received = 7
            self.stats.responses_sent = 7
        self.instances.append(self)

    def start(self) -> None:
        self.started = True

    def stop(self) -> None:
        self.stopped = True


def test_active_test_starts_host_slaves_and_confirms_lcp_data() -> None:
    FakeServer.instances.clear()
    result = run_lcp_active_rs485(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM10"),
        StationConfig(
            lcp_port="COM10",
            s1_endpoint="COM11",
            x2x_endpoint="COM15",
            shared_hmi_x2x_adapter=False,
        ),
        console_opener=FakeConsole.open_port,
        server_factory=FakeServer,
        sleep=lambda _: None,
    )
    assert result.result == TestStatus.PASS
    s1 = next(item for item in result.interfaces if item.name == "S1")
    x2x = next(item for item in result.interfaces if item.name == "X2X")
    assert s1.status == TestStatus.PASS
    assert s1.actual_values == [0x1101, 0x1102]
    assert x2x.status == TestStatus.PASS
    assert all(server.started and server.stopped for server in FakeServer.instances)


class BusyServer(FakeServer):
    def start(self) -> None:
        raise serial.SerialException("could not open port COM11: PermissionError(13, 'Access is denied')")


def test_busy_fixture_port_is_not_reported_as_dut_fail() -> None:
    result = run_lcp_active_rs485(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM10"),
        StationConfig(lcp_port="COM10", s1_endpoint="COM11"),
        console_opener=FakeConsole.open_port,
        server_factory=BusyServer,
        sleep=lambda _: None,
    )
    assert result.result == TestStatus.FIXTURE_ERROR
    s1 = next(item for item in result.interfaces if item.name == "S1")
    assert s1.status == TestStatus.FIXTURE_ERROR
    assert "Ошибка стенда" in s1.detail
