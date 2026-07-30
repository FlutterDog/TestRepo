"""Semantic evaluation of LCP Basic 1.02.0 diagnostic reports."""

from __future__ import annotations

import re
from collections.abc import Callable
from datetime import datetime

from lorentz_test.models.station import StationConfig
from lorentz_test.models.tests import CheckResult, TestStatus
from lorentz_test.protocols.lcp_diagnostic_parser import DiagnosticReport

_INTEGER = re.compile(r"[-+]?\d+")
_HEX_INTEGER = re.compile(r"0x[0-9A-Fa-f]+")
_RTC_FORMAT = "%Y-%m-%d %H:%M:%S"
_RTC_TOLERANCE_SECONDS = 300.0


def _check(
    name: str,
    expected: object,
    actual: object,
    passed: bool,
) -> CheckResult:
    return CheckResult(
        name=name,
        expected=str(expected),
        actual="missing" if actual is None else str(actual),
        status=TestStatus.PASS if passed else TestStatus.FAIL,
    )


def _skipped(name: str, expected: object, actual: object) -> CheckResult:
    return CheckResult(
        name=name,
        expected=str(expected),
        actual=str(actual),
        status=TestStatus.SKIPPED,
    )


def _first_integer(value: str | None) -> int | None:
    if value is None:
        return None
    match = _INTEGER.search(value)
    return int(match.group(0)) if match is not None else None


def _first_hex(value: str | None) -> int | None:
    if value is None:
        return None
    match = _HEX_INTEGER.search(value)
    return int(match.group(0), 16) if match is not None else None


def _status_from_checks(checks: list[CheckResult]) -> TestStatus:
    if any(item.status == TestStatus.FAIL for item in checks):
        return TestStatus.FAIL
    if any(item.status == TestStatus.PASS for item in checks):
        return TestStatus.PASS
    return TestStatus.SKIPPED


def _evaluate_rtos(report: DiagnosticReport, _: StationConfig, __: datetime) -> list[CheckResult]:
    checks: list[CheckResult] = []
    scheduler = report.one("scheduler", group="Runtime")
    checks.append(_check("rtos_scheduler", "running", scheduler, scheduler == "running"))

    sram_bytes = _first_integer(report.one("total_bytes", group="ATSAM3X8E SRAM"))
    checks.append(
        _check("atsam3x8e_sram_bytes", 98304, sram_bytes, sram_bytes == 98304)
    )

    heap_total = _first_integer(report.one("total_bytes", group="FreeRTOS heap_4"))
    heap_minimum_free = _first_integer(
        report.one("minimum_ever_free_bytes", group="FreeRTOS heap_4")
    )
    heap_ratio = (
        heap_minimum_free / heap_total
        if heap_total and heap_minimum_free is not None
        else None
    )
    checks.append(
        _check(
            "freertos_heap_minimum_free",
            ">= 10%",
            f"{heap_ratio * 100:.1f}%" if heap_ratio is not None else None,
            heap_ratio is not None and heap_ratio >= 0.10,
        )
    )

    stack_total = _first_integer(report.one("total_bytes", group="LCP task stack"))
    stack_minimum_free = _first_integer(
        report.one("minimum_free_bytes", group="LCP task stack")
    )
    stack_ratio = (
        stack_minimum_free / stack_total
        if stack_total and stack_minimum_free is not None
        else None
    )
    checks.append(
        _check(
            "lcp_task_stack_minimum_free",
            ">= 10%",
            f"{stack_ratio * 100:.1f}%" if stack_ratio is not None else None,
            stack_ratio is not None and stack_ratio >= 0.10,
        )
    )
    return checks


def _evaluate_config(
    report: DiagnosticReport,
    _: StationConfig,
    __: datetime,
) -> list[CheckResult]:
    checks: list[CheckResult] = []
    flash_start = _first_hex(report.one("flash_start", group="Active configuration"))
    flash_bytes = _first_integer(report.one("flash_bytes", group="Active configuration"))
    actual_region = (
        f"0x{flash_start:08X} / {flash_bytes} bytes"
        if flash_start is not None and flash_bytes is not None
        else None
    )
    checks.append(
        _check(
            "config_flash_region",
            "0x000F8000 / 32768 bytes",
            actual_region,
            flash_start == 0x000F8000 and flash_bytes == 32768,
        )
    )

    busy = report.one("busy", group="Operation")
    operation = report.one("operation", group="Operation")
    flash_result = report.one("flash_result", group="Operation")
    checks.append(
        _check(
            "config_service_idle",
            "busy=no, operation=none, flash_result=ok",
            f"busy={busy}, operation={operation}, flash_result={flash_result}",
            busy == "no" and operation == "none" and flash_result == "ok",
        )
    )

    source = report.one("source", group="Active configuration")
    slot_a_valid = report.one("valid", group="Slot A")
    slot_b_valid = report.one("valid", group="Slot B")
    if source and "internal flash" in source.casefold():
        checks.append(
            _check(
                "config_active_flash_slot",
                "at least one valid slot",
                f"A={slot_a_valid}, B={slot_b_valid}",
                "yes" in (slot_a_valid, slot_b_valid),
            )
        )
    else:
        checks.append(
            _skipped(
                "config_active_flash_slot",
                "internal Flash source",
                source or "runtime defaults",
            )
        )

    slot_a_flash = report.one("flash", group="Slot A")
    slot_b_flash = report.one("flash", group="Slot B")
    checks.append(
        _check(
            "config_slot_flash_access",
            "A=ok, B=ok",
            f"A={slot_a_flash}, B={slot_b_flash}",
            slot_a_flash == "ok" and slot_b_flash == "ok",
        )
    )
    return checks


def _evaluate_field(
    report: DiagnosticReport,
    station: StationConfig,
    _: datetime,
) -> list[CheckResult]:
    checks: list[CheckResult] = []
    service = report.one("service")
    checks.append(_check("field_service", "running", service, service == "running"))

    endpoints = (
        station.s1_endpoint,
        station.s2_endpoint,
        station.s3_endpoint,
        station.s4_endpoint,
    )
    for index, endpoint in enumerate(endpoints, start=1):
        group = f"S{index}"
        present = report.one("present", group=group)
        uart_errors = _first_integer(report.one("uart_errors", group=group))
        checks.append(
            _check(f"s{index}_uart_present", "yes", present, present == "yes")
        )
        checks.append(
            _check(f"s{index}_uart_errors", 0, uart_errors, uart_errors == 0)
        )

        connection = report.one("connection", group=group)
        valid = report.one("valid", group=group)
        last_result = report.one("last_result", group=group)
        success = _first_integer(report.one("success", group=group))
        if endpoint:
            actual = (
                f"connection={connection}, valid={valid}, last_result={last_result}, "
                f"success={success}"
            )
            checks.append(
                _check(
                    f"s{index}_external_modbus",
                    "online, valid=yes, result=ok, success>0",
                    actual,
                    connection == "online"
                    and valid == "yes"
                    and last_result == "ok"
                    and (success or 0) > 0,
                )
            )
        else:
            checks.append(
                _skipped(
                    f"s{index}_external_modbus",
                    "configured station endpoint",
                    "endpoint not configured",
                )
            )
    return checks


def _evaluate_rs485(
    report: DiagnosticReport,
    _: StationConfig,
    __: datetime,
) -> list[CheckResult]:
    checks: list[CheckResult] = []
    owner = report.one("owner", group="X2X physical port")
    echo = report.one("echo", group="X2X physical port")
    checks.append(
        _check(
            "x2x_port_owner",
            "X2X master / echo disabled",
            f"{owner} / {echo}",
            owner == "X2X master" and echo == "disabled",
        )
    )

    errors: list[tuple[str, int | None]] = []
    for item in report.entries:
        if item.key == "uart_errors":
            errors.append((item.scope or item.group or "unknown", _first_integer(item.value)))
    actual_errors = ", ".join(f"{name}={value}" for name, value in errors)
    checks.append(
        _check(
            "rs485_uart_errors",
            "all zero",
            actual_errors or "missing",
            bool(errors) and all(value == 0 for _, value in errors),
        )
    )
    return checks


def _evaluate_sc16is(
    report: DiagnosticReport,
    _: StationConfig,
    __: datetime,
) -> list[CheckResult]:
    lowered = report.raw_output.casefold()
    checks: list[CheckResult] = []
    probe_complete = "probe_complete=yes" in lowered
    checks.append(
        _check("sc16is_probe_complete", "yes", "yes" if probe_complete else "no", probe_complete)
    )

    required = (
        "uart1_cs channel a=present",
        "uart2_cs channel a=present",
        "uart3_cs channel a=present",
        "pc: present=yes",
        "hmi: present=yes",
        "s1: present=yes",
    )
    missing = [item for item in required if item not in lowered]
    checks.append(
        _check(
            "sc16is_required_channels",
            "UART1/2/3 A and PC/HMI/S1 present",
            "all present" if not missing else "missing: " + ", ".join(missing),
            not missing,
        )
    )

    channel_b_absent = "uart2_cs channel b=absent" in lowered
    checks.append(
        _check(
            "sc16is_uart2_channel_b",
            "absent",
            "absent" if channel_b_absent else "unexpected",
            channel_b_absent,
        )
    )
    return checks


def _evaluate_x2x(
    report: DiagnosticReport,
    _: StationConfig,
    __: datetime,
) -> list[CheckResult]:
    checks: list[CheckResult] = []
    loaded = report.one("loaded", group="Configuration")
    config_result = report.one("result", group="Configuration")
    registry = report.one("registry", group="Configuration")
    checks.append(
        _check(
            "x2x_configuration",
            "loaded=yes, result=ok, registry=ok",
            f"loaded={loaded}, result={config_result}, registry={registry}",
            loaded == "yes" and config_result == "ok" and registry == "ok",
        )
    )

    owner = report.one("port_owner", group="Runtime")
    crc_self_test = report.one("crc_self_test", group="Runtime")
    uart_errors = _first_integer(report.one("uart_errors", group="Runtime"))
    checks.append(
        _check(
            "x2x_transport",
            "owner=X2X master, crc=ok, uart_errors=0",
            f"owner={owner}, crc={crc_self_test}, uart_errors={uart_errors}",
            owner == "X2X master" and crc_self_test == "ok" and uart_errors == 0,
        )
    )

    module_count = _first_integer(report.one("module_count", group="Runtime"))
    if module_count is None:
        checks.append(_check("x2x_module_count", "present", None, False))
        return checks
    if module_count == 0:
        checks.append(
            _skipped(
                "x2x_module_communication",
                "configured modules",
                "module_count=0",
            )
        )
        return checks

    module_groups = report.group_names("MODULE ")
    checks.append(
        _check(
            "x2x_module_registry_count",
            module_count,
            len(module_groups),
            len(module_groups) == module_count,
        )
    )
    for group in module_groups:
        slave = report.one("slave", group=group)
        type_name = report.one("type_name", group=group)
        connection = report.one("connection", group=group)
        device_fault = report.one("device_fault", group=group)
        communication_error = report.one("communication_error", group=group)
        success = _first_integer(report.one("success", group=group))
        consecutive_failures = _first_integer(
            report.one("consecutive_failures", group=group)
        )
        actual = (
            f"slave={slave}, type={type_name}, connection={connection}, "
            f"fault={device_fault}, error={communication_error}, success={success}, "
            f"consecutive_failures={consecutive_failures}"
        )
        checks.append(
            _check(
                f"x2x_module_{slave or group}",
                "online, no fault, communication ok, success>0, consecutive_failures=0",
                actual,
                connection == "online"
                and device_fault == "no"
                and communication_error == "ok"
                and (success or 0) > 0
                and consecutive_failures == 0,
            )
        )
    return checks


def _evaluate_ethernet(
    report: DiagnosticReport,
    _: StationConfig,
    __: datetime,
) -> list[CheckResult]:
    checks: list[CheckResult] = []
    service_reload = report.one("service_reload")
    checks.append(
        _check(
            "ethernet_service_reload",
            "idle",
            service_reload,
            service_reload == "idle",
        )
    )

    for group in ("ETH1", "ETH2"):
        initialized = report.one("initialized", group=group)
        init_result = report.one("init", group=group)
        version_register = report.one("versionr", group=group)
        transport_errors = _first_integer(
            report.one("transport_errors", group=group)
        )
        checks.append(
            _check(
                f"{group.casefold()}_controller",
                "initialized=yes, init=ok, VERSIONR=0x04, transport_errors=0",
                (
                    f"initialized={initialized}, init={init_result}, "
                    f"VERSIONR={version_register}, transport_errors={transport_errors}"
                ),
                initialized == "yes"
                and init_result == "ok"
                and version_register == "0x04"
                and transport_errors == 0,
            )
        )

        link = report.one("link", group=group)
        if link == "up":
            checks.append(_check(f"{group.casefold()}_link", "up", link, True))
        else:
            checks.append(
                _skipped(
                    f"{group.casefold()}_link",
                    "external Ethernet fixture connected",
                    f"link={link or 'missing'}",
                )
            )
    return checks


def _evaluate_sd(
    report: DiagnosticReport,
    _: StationConfig,
    __: datetime,
) -> list[CheckResult]:
    checks: list[CheckResult] = []
    detect_stable = report.one("detect_stable")
    detected = report.one("detect_debounced")
    checks.append(
        _check(
            "sd_detect_stable",
            "yes",
            detect_stable,
            detect_stable == "yes",
        )
    )

    if detected == "inserted":
        ready = report.one("ready")
        last_result = report.one("last_result")
        fat_type = report.one("fat_type")
        test_file_exists = report.one("sdtest.txt_exists")
        checks.append(
            _check(
                "sd_filesystem",
                "ready=yes, last_result=ok, FAT16/FAT32, SDTEST.TXT_exists=yes",
                (
                    f"ready={ready}, last_result={last_result}, fat_type={fat_type}, "
                    f"SDTEST.TXT_exists={test_file_exists}"
                ),
                ready == "yes"
                and last_result == "ok"
                and fat_type in ("FAT16", "FAT32")
                and test_file_exists == "yes",
            )
        )
    else:
        checks.append(
            _skipped(
                "sd_filesystem",
                "test card inserted",
                f"detect={detected or 'missing'}",
            )
        )
    return checks


def _evaluate_battery(
    report: DiagnosticReport,
    _: StationConfig,
    __: datetime,
) -> list[CheckResult]:
    checks: list[CheckResult] = []
    stable = report.one("stable")
    instant_state = report.one("instant_state")
    debounced_state = report.one("debounced_state")
    checks.append(
        _check("battery_input_stable", "yes", stable, stable == "yes")
    )
    checks.append(
        _check(
            "battery_state",
            "instant=ok, debounced=ok",
            f"instant={instant_state}, debounced={debounced_state}",
            instant_state == "ok" and debounced_state == "ok",
        )
    )
    return checks


def _evaluate_rtc(
    report: DiagnosticReport,
    _: StationConfig,
    now_local: datetime,
) -> list[CheckResult]:
    checks: list[CheckResult] = []
    read_result = report.one("read_result")
    registers_valid = report.one("registers_valid")
    range_valid = report.one("range_valid")
    checks.append(
        _check(
            "rtc_registers",
            "read_result=ok, registers_valid=yes, range_valid=yes",
            (
                f"read_result={read_result}, registers_valid={registers_valid}, "
                f"range_valid={range_valid}"
            ),
            read_result == "ok"
            and registers_valid == "yes"
            and range_valid == "yes",
        )
    )

    slow_clock = report.one("slow_clock_source")
    checks.append(
        _check(
            "rtc_slow_clock",
            "external 32.768 kHz crystal",
            slow_clock,
            slow_clock == "external 32.768 kHz crystal",
        )
    )

    rtc_text = report.one("datetime")
    rtc_datetime: datetime | None = None
    if rtc_text:
        try:
            rtc_datetime = datetime.strptime(rtc_text, _RTC_FORMAT)
        except ValueError:
            rtc_datetime = None
    difference = (
        abs((now_local - rtc_datetime).total_seconds())
        if rtc_datetime is not None
        else None
    )
    checks.append(
        _check(
            "rtc_time_matches_pc",
            f"difference <= {_RTC_TOLERANCE_SECONDS:.0f} s",
            (
                f"{rtc_text}; difference={difference:.0f} s"
                if difference is not None
                else rtc_text
            ),
            difference is not None and difference <= _RTC_TOLERANCE_SECONDS,
        )
    )
    return checks


def _evaluate_watchdog(
    report: DiagnosticReport,
    _: StationConfig,
    __: datetime,
) -> list[CheckResult]:
    checks: list[CheckResult] = []
    enabled = report.one("enabled", group="Runtime")
    timeout_ms = _first_integer(report.one("timeout_ms", group="Runtime"))
    feed_period_ms = _first_integer(report.one("feed_period_ms", group="Runtime"))
    test_reset_armed = report.one("test_reset_armed", group="Runtime")
    checks.append(
        _check(
            "watchdog_runtime",
            "enabled=yes, 0 < feed_period < timeout, test_reset_armed=no",
            (
                f"enabled={enabled}, timeout_ms={timeout_ms}, "
                f"feed_period_ms={feed_period_ms}, test_reset_armed={test_reset_armed}"
            ),
            enabled == "yes"
            and timeout_ms is not None
            and feed_period_ms is not None
            and 0 < feed_period_ms < timeout_ms
            and test_reset_armed == "no",
        )
    )

    status_register = _first_hex(report.one("wdt_sr", group="Raw registers"))
    checks.append(
        _check(
            "watchdog_status_register",
            "0x0",
            f"0x{status_register:X}" if status_register is not None else None,
            status_register == 0,
        )
    )

    recovery = report.one("usb_recovery_after_watchdog", group="Runtime")
    if recovery == "performed":
        checks.append(
            _check("watchdog_reset_recovery", "performed", recovery, True)
        )
    else:
        checks.append(
            _skipped(
                "watchdog_reset_recovery",
                "destructive watchdog reset test",
                recovery or "not performed",
            )
        )
    return checks


Evaluator = Callable[[DiagnosticReport, StationConfig, datetime], list[CheckResult]]

_EVALUATORS: dict[str, Evaluator] = {
    "rtos": _evaluate_rtos,
    "config_status": _evaluate_config,
    "field": _evaluate_field,
    "rs485": _evaluate_rs485,
    "sc16is": _evaluate_sc16is,
    "x2x": _evaluate_x2x,
    "ethernet": _evaluate_ethernet,
    "sd": _evaluate_sd,
    "battery": _evaluate_battery,
    "rtc": _evaluate_rtc,
    "watchdog": _evaluate_watchdog,
}


def evaluate_diagnostic_command(
    command_id: str,
    raw_output: str,
    station: StationConfig,
    *,
    now_local: datetime | None = None,
) -> tuple[TestStatus, list[CheckResult]]:
    report = DiagnosticReport(raw_output)
    evaluator = _EVALUATORS.get(command_id)
    if evaluator is None:
        checks = [_check("response_received", "non-empty response", "received", bool(raw_output))]
    else:
        local_time = now_local or datetime.now().astimezone().replace(tzinfo=None)
        checks = evaluator(report, station, local_time)
    return _status_from_checks(checks), checks
