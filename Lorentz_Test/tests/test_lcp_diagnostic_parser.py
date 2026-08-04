from lorentz_test.protocols.lcp_diagnostic_parser import (
    DiagnosticReport,
    parse_diagnostic_output,
    qualified_values,
)


def test_parser_preserves_repeated_keys_by_group() -> None:
    text = (
        "-- ATSAM3X8E SRAM --\r\n"
        "total_bytes = 98304\r\n"
        "-- FreeRTOS heap_4 --\r\n"
        "total_bytes = 32768\r\n"
        "-- LCP task stack --\r\n"
        "total_bytes = 8192\r\n"
    )
    values = qualified_values(parse_diagnostic_output(text))
    assert values["ATSAM3X8E SRAM.total_bytes"] == "98304"
    assert values["FreeRTOS heap_4.total_bytes"] == "32768"
    assert values["LCP task stack.total_bytes"] == "8192"


def test_parser_splits_multiple_assignments_on_one_line() -> None:
    report = DiagnosticReport(
        "-- Runtime --\r\n"
        "module_count = 1, current_slave = 1\r\n"
        "paused = no, pause_pending = no, port_owner = X2X master\r\n"
    )
    assert report.one("module_count", group="Runtime") == "1"
    assert report.one("current_slave", group="Runtime") == "1"
    assert report.one("port_owner", group="Runtime") == "X2X master"


def test_parser_inherits_rs485_subgroup_for_indented_line() -> None:
    report = DiagnosticReport(
        "-- FieldSensor physical ports --\r\n"
        "S2: hardware = Serial1, serial = 9600 8N1\r\n"
        "  owner = FieldSensor master, uart_errors = 0\r\n"
    )
    assert report.one("hardware", group="FieldSensor physical ports", scope="S2") == "Serial1"
    assert report.one("owner", group="FieldSensor physical ports", scope="S2") == "FieldSensor master"
    assert report.one("uart_errors", group="FieldSensor physical ports", scope="S2") == "0"
