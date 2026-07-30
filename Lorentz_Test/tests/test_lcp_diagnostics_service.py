from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import LcpHelloRequest, TestStatus as Status
from lorentz_test.services.lcp_diagnostics import run_lcp_diagnostic_snapshot


class FakeConsole:
    fail_command: str | None = None

    def __init__(self) -> None:
        self.closed = False
        self.commands: list[str] = []

    @classmethod
    def open_port(cls, *_: object, **__: object) -> "FakeConsole":
        return cls()

    def execute(self, command: str) -> str:
        self.commands.append(command)
        if command == self.fail_command:
            raise RuntimeError("simulated diagnostic failure")
        return f"section = {command}\r\nstatus = captured\r\n"

    def close(self) -> None:
        self.closed = True


class FailingConsole(FakeConsole):
    fail_command = "x2x"



def test_diagnostic_snapshot_captures_all_commands() -> None:
    result = run_lcp_diagnostic_snapshot(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator", port="COM7"),
        StationConfig(),
        console_opener=FakeConsole.open_port,
    )
    assert result.result == Status.PASS
    assert result.evaluation_mode == "capture_only"
    assert len(result.commands) == 11
    assert all(item.status == Status.PASS for item in result.commands)
    assert result.commands[0].parsed_values["status"] == "captured"



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
    assert "simulated diagnostic failure" in (failed[0].error or "")



def test_diagnostic_snapshot_requires_port() -> None:
    result = run_lcp_diagnostic_snapshot(
        LcpHelloRequest(serial_number="LCP-1", operator="Operator"),
        StationConfig(lcp_port=None),
        console_opener=FakeConsole.open_port,
    )
    assert result.result == Status.FAIL
    assert "not configured" in (result.error or "")
