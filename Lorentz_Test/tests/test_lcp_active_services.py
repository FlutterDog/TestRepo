import struct
from datetime import datetime, timedelta
from types import SimpleNamespace

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpHelloRequest, TestStatus
from lorentz_test.protocols.lcp_usb import (
    COMMAND_GET_CONFIG,
    COMMAND_VALIDATE_CONFIG,
    Frame,
    STATUS_OK,
    crc32,
)
from lorentz_test.services.lcp_active_services import run_lcp_active_services


class FakeClient:
    def __init__(self, *_: object, **__: object) -> None:
        bundle = bytes(104)
        checksum = crc32(bundle)
        self.payload = (
            struct.pack("<HHIII", 1, 104, 1, 2, checksum)
            + bytes((1, 0, 0, 0))
            + bundle
        )

    def __enter__(self) -> "FakeClient":
        return self

    def __exit__(self, *_: object) -> None:
        pass

    def hello(self) -> SimpleNamespace:
        return SimpleNamespace(schema_version=1, bundle_size=104)

    def request(self, command: int, payload: bytes = b"", **_: object) -> Frame:
        if command == COMMAND_GET_CONFIG:
            return Frame(command, STATUS_OK, 0, 1, self.payload)
        if command == COMMAND_VALIDATE_CONFIG:
            assert len(payload) == 112
            return Frame(command, STATUS_OK, 0, 2, b"")
        raise AssertionError(command)


class FakeConsole:
    def __init__(self) -> None:
        self.rtos_calls = 0
        self.rtc_requested: datetime | None = None
        self.closed = False

    @classmethod
    def open_port(cls, *_: object, **__: object) -> "FakeConsole":
        return cls()

    def execute(self, command: str) -> str:
        if command == "sd":
            return """
[ MICROSD ]
-- Filesystem --
ready=yes, last_result=ok
fat_type=FAT32, SDTEST.TXT_exists=yes
"""
        if command == "sd test":
            return """
write_result=ok
read_result=ok
loaded_count=3
data_match=yes
result=OK
"""
        if command.startswith("rtc set "):
            self.rtc_requested = datetime.strptime(command[8:], "%Y-%m-%d %H:%M:%S")
            return "rtc requested_datetime=2026-07-30 20:00:00\r\nrtc update_start_result=ok\r\n"
        if command == "rtc":
            assert self.rtc_requested is not None
            value = self.rtc_requested
            return f"""
[ LOCAL RTC ]
-- Clock and update state --
update_state=idle, update_result=ok
-- Current date and time --
datetime={value:%Y-%m-%d %H:%M:%S}
"""
        if command == "time":
            assert self.rtc_requested is not None
            value = self.rtc_requested + timedelta(seconds=3)
            return f"[ RTC TIME ]\ndatetime={value:%Y-%m-%d %H:%M:%S}\n"
        if command == "rtos":
            self.rtos_calls += 1
            tick = 1000 if self.rtos_calls == 1 else 3000
            uptime = 10000 if self.rtos_calls == 1 else 12000
            return f"""
-- Runtime --
scheduler = running
tick_count = {tick}
uptime_ms = {uptime}
-- FreeRTOS heap_4 --
minimum_ever_free_bytes = 22480 (68.6%)
-- LCP task stack --
minimum_free_bytes = 7728 (94.3%)
"""
        raise AssertionError(command)

    def close(self) -> None:
        self.closed = True


def test_active_services_are_safe_and_pass_with_reference_outputs() -> None:
    result = run_lcp_active_services(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM10"),
        StationConfig(lcp_port="COM10"),
        client_factory=FakeClient,
        console_opener=FakeConsole.open_port,
        sleep=lambda _: None,
    )

    assert result.result == TestStatus.PASS
    assert [step.status for step in result.steps] == [
        TestStatus.PASS,
        TestStatus.PASS,
        TestStatus.PASS,
        TestStatus.PASS,
    ]
    config_step = result.steps[0]
    assert "no Flash write was issued" in config_step.detail
