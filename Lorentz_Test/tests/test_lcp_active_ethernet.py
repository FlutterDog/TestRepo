from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpHelloRequest, TestStatus
from lorentz_test.protocols.modbus_tcp_client import ModbusTcpReadResult
from lorentz_test.services.lcp_active_ethernet import run_lcp_active_ethernet


ETH_BEFORE = """
[ ETHERNET MODBUS TCP ]
-- ETH1 --
initialized = yes, init = ok, link = up
ip = 192.168.1.16
requests = 4, responses = 4
transport_errors = 0
-- ETH2 --
initialized = yes, init = ok, link = down
ip = 192.168.1.1
requests = 0, responses = 0
transport_errors = 0
"""

ETH_AFTER = """
[ ETHERNET MODBUS TCP ]
-- ETH1 --
initialized = yes, init = ok, link = up
ip = 192.168.1.16
requests = 5, responses = 5
transport_errors = 0
-- ETH2 --
initialized = yes, init = ok, link = down
ip = 192.168.1.1
requests = 0, responses = 0
transport_errors = 0
"""


class FakeConsole:
    def __init__(self) -> None:
        self.calls = 0
        self.closed = False

    @classmethod
    def open_port(cls, *_: object, **__: object) -> "FakeConsole":
        return cls()

    def execute(self, command: str) -> str:
        assert command == "eth"
        self.calls += 1
        return ETH_BEFORE if self.calls == 1 else ETH_AFTER

    def close(self) -> None:
        self.closed = True


def fake_reader(host: str, **kwargs: object) -> ModbusTcpReadResult:
    assert host == "192.168.1.16"
    assert kwargs["start_address"] == 0
    assert kwargs["register_count"] == 12
    return ModbusTcpReadResult(
        transaction_id=int(kwargs["transaction_id"]),
        unit_id=1,
        registers=list(range(12)),
    )


def test_active_ethernet_confirms_fc03_and_dut_counters() -> None:
    result = run_lcp_active_ethernet(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM10"),
        StationConfig(
            lcp_port="COM10",
            eth1_ip="192.168.1.16",
            eth2_ip="192.168.1.1",
            eth1_test_enabled=True,
            eth2_test_enabled=False,
        ),
        console_opener=FakeConsole.open_port,
        tcp_reader=fake_reader,
    )

    assert result.result == TestStatus.PASS
    eth1 = next(item for item in result.interfaces if item.name == "ETH1")
    eth2 = next(item for item in result.interfaces if item.name == "ETH2")
    assert eth1.status == TestStatus.PASS
    assert eth1.registers == list(range(12))
    assert eth1.before_requests == 4
    assert eth1.after_requests == 5
    assert eth2.status == TestStatus.SKIPPED


def test_link_down_is_fixture_error_not_dut_fail() -> None:
    result = run_lcp_active_ethernet(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM10"),
        StationConfig(
            lcp_port="COM10",
            eth1_ip="192.168.1.16",
            eth2_ip="192.168.1.1",
            eth1_test_enabled=False,
            eth2_test_enabled=True,
        ),
        console_opener=FakeConsole.open_port,
        tcp_reader=fake_reader,
    )

    assert result.result == TestStatus.FIXTURE_ERROR
    eth2 = next(item for item in result.interfaces if item.name == "ETH2")
    assert eth2.status == TestStatus.FIXTURE_ERROR
    assert "link=down" in eth2.detail
