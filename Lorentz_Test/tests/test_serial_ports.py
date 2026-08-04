from types import SimpleNamespace

from lorentz_test.serial_ports import _port_sort_key, _to_info


def test_windows_com_ports_sort_numerically() -> None:
    assert sorted(["COM10", "COM2", "COM1"], key=_port_sort_key) == ["COM1", "COM2", "COM10"]


def test_lcp_candidate_is_only_a_hint() -> None:
    item = SimpleNamespace(
        device="COM7",
        description="Arduino Due Programming Port",
        hwid="USB VID:PID=2341:003D",
        vid=0x2341,
        pid=0x003D,
        serial_number=None,
        manufacturer="Arduino LLC",
        product="Arduino Due",
        interface=None,
        location="1-2",
    )
    assert _to_info(item).lcp_candidate is True
