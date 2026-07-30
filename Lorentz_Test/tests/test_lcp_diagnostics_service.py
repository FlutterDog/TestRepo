from datetime import datetime

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpHelloRequest, TestStatus as Status
from lorentz_test.services.lcp_diagnostics import run_lcp_diagnostic_snapshot


def _pass_responses() -> dict[str, str]:
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    field_groups = "".join(
        f"-- S{index} --\r\npresent = yes, uart_errors = 0\r\n"
        for index in range(1, 5)
    )
    return {
        "rtos": (
            "-- Runtime --\r\nscheduler = running\r\n"
            "-- ATSAM3X8E SRAM --\r\ntotal_bytes = 98304\r\n"
            "-- FreeRTOS heap_4 --\r\ntotal_bytes = 32768\r\n"
            "minimum_ever_free_bytes = 20000\r\n"
            "-- LCP task stack --\r\ntotal_bytes = 8192\r\n"
            "minimum_free_bytes = 4096\r\n"
        ),
        "config status": (
            "-- Active configuration --\r\nsource = internal Flash, slot = A\r\n"
            "flash_start = 0x000F8000, flash_bytes = 32768\r\n"
            "-- Slot A --\r\nvalid = yes, flash = ok\r\n"
            "-- Slot B --\r\nvalid = no, flash = ok\r\n"
            "-- Operation --\r\nbusy = no, operation = none, flash_result = ok\r\n"
        ),
        "field": "service = running\r\n" + field_groups,
        "rs485": (
            "-- X2X physical port --\r\n"
            "owner = X2X master, echo = disabled, uart_errors = 0\r\n"
            "-- FieldSensor physical ports --\r\n"
            "S2: owner = FieldSensor master, uart_errors = 0\r\n"
            "S3: owner = FieldSensor master, uart_errors = 0\r\n"
            "S4: owner = FieldSensor master, uart_errors = 0\r\n"
        ),
        "sc16is": (
            "probe_complete=yes\r\n"
            "UART1_CS channel A=present\r\n"
            "UART2_CS channel A=present\r\n"
            "UART2_CS channel B=absent\r\n"
            "UART3_CS channel A=present\r\n"
            "PC: present=yes\r\nHMI: present=yes\r\nS1: present=yes\r\n"
        ),
        "x2x": (
            "-- Configuration --\r\nloaded = yes, result = ok, registry = ok\r\n"
            "-- Runtime --\r\nmodule_count = 0, port_owner = X2X master\r\n"
            "crc_self_test = ok, uart_errors = 0\r\n"
        ),
        "eth": (
            "service_reload = idle\r\n"
            "-- ETH1 --\r\ninitialized = yes, init = ok, link = down\r\n"
            "VERSIONR = 0x04, transport_errors = 0\r\n"
            "-- ETH2 --\r\ninitialized = yes, init = ok, link = down\r\n"
            "VERSIONR = 0x04, transport_errors = 0\r\n"
        ),
        "sd": (
            "detect_stable=yes, detect_debounced=inserted\r\n"
            "ready=yes, last_result=ok\r\n"
            "fat_type=FAT32, SDTEST.TXT_exists=yes\r\n"
        ),
        "battery": (
            "stable = yes\r\ninstant_state = ok, debounced_state = ok\r\n"
        ),
        "rtc": (
            "slow_clock_source=external 32.768 kHz crystal\r\n"
            "read_result=ok\r\n"
            f"datetime={now}\r\n"
            "registers_valid=yes, range_valid=yes\r\n"
        ),
        "watchdog": (
            "-- Runtime --\r\nenabled = yes, timeout_ms = 4000\r\n"
            "feed_period_ms = 500, test_reset_armed = no\r\n"
            "usb_recovery_after_watchdog = not performed\r\n"
            "-- Raw registers --\r\nWDT_SR = 0x0\r\n"
        ),
    }


class FakeConsole:
    fail_command: str | None = None

    def __init__(self) -> None:
        self.closed = False
        self.commands: list[str] = []
        self.responses = _pass_responses()

    @classmethod
    def open_port(cls, *_: object, **__: object) -> "FakeConsole":
        return cls()

    def execute(self, command: str) -> str:
        self.commands.append(command)
        if command == self.fail_command:
            raise RuntimeError("simulated diagnostic failure")
        return self.responses[command]

    def close(self) -> None:
        self.closed = True


class FailingConsole(FakeConsole):
    fail_command = "x2x"


class LostX2XConsole(FakeConsole):
    def execute(self, command: str) -> str:
        if command == "x2x":
            return (
                "-- Configuration --\r\nloaded = yes, result = ok, registry = ok\r\n"
                "-- Runtime --\r\nmodule_count = 1, port_owner = X2X master\r\n"
                "crc_self_test = ok, uart_errors = 0\r\n"
                "-- MODULE slave = 1 --\r\n"
                "slave = 1, type_name = LCT1114_2\r\n"
                "connection = lost, device_fault = no\r\n"
                "communication_error = timeout, success = 0\r\n"
                "consecutive_failures = 255\r\n"
            )
        return super().execute(command)


def test_diagnostic_snapshot_evaluates_all_commands() -> None:
    result = run_lcp_diagnostic_snapshot(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM7"),
        StationConfig(),
        console_opener=FakeConsole.open_port,
    )
    assert result.result == Status.PASS
    assert result.evaluation_mode == "semantic_v1.1"
    assert len(result.commands) == 11
    assert all(item.capture_status == Status.PASS for item in result.commands)
    assert all(item.status == Status.PASS for item in result.commands)
    field = next(item for item in result.commands if item.command_id == "field")
    assert any(check.status == Status.SKIPPED for check in field.checks)
    assert "Runtime.scheduler" in result.commands[0].parsed_values


def test_passive_snapshot_skips_configured_external_s_ports() -> None:
    result = run_lcp_diagnostic_snapshot(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM7"),
        StationConfig(s1_endpoint="COM11", s2_endpoint="COM12", s3_endpoint="COM13"),
        console_opener=FakeConsole.open_port,
    )
    field = next(item for item in result.commands if item.command_id == "field")
    external = [check for check in field.checks if check.name.endswith("_external_modbus")]
    assert len(external) == 4
    assert all(check.status == Status.SKIPPED for check in external)
    assert all("passive snapshot only" in check.actual for check in external)


def test_diagnostic_snapshot_continues_after_one_command_failure() -> None:
    result = run_lcp_diagnostic_snapshot(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM7"),
        StationConfig(),
        console_opener=FailingConsole.open_port,
    )
    assert result.result == Status.FAIL
    assert len(result.commands) == 11
    failed = [item for item in result.commands if item.status == Status.FAIL]
    assert len(failed) == 1
    assert failed[0].command == "x2x"
    assert failed[0].capture_status == Status.FAIL
    assert "simulated diagnostic failure" in (failed[0].error or "")


def test_diagnostic_snapshot_fails_lost_configured_x2x_module() -> None:
    result = run_lcp_diagnostic_snapshot(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM7"),
        StationConfig(),
        console_opener=LostX2XConsole.open_port,
    )
    assert result.result == Status.FAIL
    x2x = next(item for item in result.commands if item.command_id == "x2x")
    assert x2x.capture_status == Status.PASS
    assert x2x.status == Status.FAIL
    assert any(check.name == "x2x_module_1" and check.status == Status.FAIL for check in x2x.checks)


def test_diagnostic_snapshot_requires_port() -> None:
    result = run_lcp_diagnostic_snapshot(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator"),
        StationConfig(lcp_port=None),
        console_opener=FakeConsole.open_port,
    )
    assert result.result == Status.FAIL
    assert "not configured" in (result.error or "")
