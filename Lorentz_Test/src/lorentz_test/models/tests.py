"""Test request and result models."""

from __future__ import annotations

from datetime import datetime
from enum import Enum

from pydantic import BaseModel, ConfigDict, Field


class TestStatus(str, Enum):
    WAITING = "WAITING"
    RUNNING = "RUNNING"
    PASS = "PASS"
    FAIL = "FAIL"
    FIXTURE_ERROR = "FIXTURE_ERROR"
    SKIPPED = "SKIPPED"


class EndpointAccessStatus(str, Enum):
    AVAILABLE = "AVAILABLE"
    BUSY = "BUSY"
    NOT_FOUND = "NOT_FOUND"
    UNREACHABLE = "UNREACHABLE"
    UNSUPPORTED = "UNSUPPORTED"
    ERROR = "ERROR"
    SKIPPED = "SKIPPED"


class LcpHelloRequest(BaseModel):
    model_config = ConfigDict(extra="forbid", str_strip_whitespace=True)

    serial_number: str = Field(min_length=1, max_length=80)
    operator: str = Field(min_length=1, max_length=80)
    port: str | None = None


class ConfirmedTestRequest(LcpHelloRequest):
    confirmation: str = Field(min_length=1, max_length=40)


class CheckResult(BaseModel):
    name: str
    expected: str
    actual: str
    status: TestStatus


class EndpointAccessResult(BaseModel):
    name: str
    endpoint: str | None = None
    status: EndpointAccessStatus
    detail: str


class HelloPayload(BaseModel):
    protocol_version: int
    schema_version: int
    bundle_size: int
    max_payload: int
    capabilities: int
    storage_version: int
    header_size: int


class LcpHelloResult(BaseModel):
    utility_version: str = "0.8.0"
    test_profile_version: str = "lcp2116-usb-identity-v1"
    test_id: str = "lcp_usb_identity"
    device_type: str = "LCP2116"
    result: TestStatus
    started_at: datetime
    duration_ms: int
    station_name: str
    serial_number: str
    operator: str
    port: str
    hello: HelloPayload | None = None
    checks: list[CheckResult] = Field(default_factory=list)
    expected_firmware_version: str = "1.02.0"
    firmware_name: str | None = None
    firmware_version: str | None = None
    firmware_stage: str | None = None
    firmware_target: str | None = None
    firmware_version_verified: bool = False
    firmware_note: str = "Версия firmware читается через диагностическую консоль USB."
    firmware_raw_output: str | None = None
    error: str | None = None
    report_file: str | None = None


class DiagnosticParsedValue(BaseModel):
    section: str | None = None
    group: str | None = None
    scope: str | None = None
    key: str
    value: str
    line_number: int


class DiagnosticCommandResult(BaseModel):
    command_id: str
    command: str
    title: str
    capture_status: TestStatus = TestStatus.PASS
    status: TestStatus
    duration_ms: int
    checks: list[CheckResult] = Field(default_factory=list)
    parsed_values: dict[str, str] = Field(default_factory=dict)
    parsed_entries: list[DiagnosticParsedValue] = Field(default_factory=list)
    raw_output: str | None = None
    error: str | None = None


class LcpDiagnosticsResult(BaseModel):
    utility_version: str = "0.8.0"
    test_profile_version: str = "lcp2116-diagnostic-semantic-v1.2"
    test_id: str = "lcp_diagnostic_snapshot"
    device_type: str = "LCP2116"
    result: TestStatus
    started_at: datetime
    duration_ms: int
    station_name: str
    serial_number: str
    operator: str
    port: str
    evaluation_mode: str = "semantic_v1.2"
    endpoint_access: list[EndpointAccessResult] = Field(default_factory=list)
    commands: list[DiagnosticCommandResult] = Field(default_factory=list)
    note: str = (
        "Статус команды учитывает внутреннюю диагностику LCP. Внешние S1–S4, "
        "X2X module communication и абсолютное время RTC в пассивном снимке "
        "отмечаются SKIPPED до активных тестов."
    )
    error: str | None = None
    report_file: str | None = None


class ActiveInterfaceResult(BaseModel):
    name: str
    endpoint: str | None = None
    role: str
    serial: str | None = None
    status: TestStatus
    requests_received: int = 0
    responses_sent: int = 0
    crc_errors: int = 0
    protocol_errors: int = 0
    io_errors: int = 0
    fixture_last_error: str | None = None
    before_success: int | None = None
    after_success: int | None = None
    expected_values: list[int] = Field(default_factory=list)
    actual_values: list[int] = Field(default_factory=list)
    observed_requests: list[str] = Field(default_factory=list)
    detail: str


class LcpActiveRs485Result(BaseModel):
    utility_version: str = "0.8.0"
    test_profile_version: str = "lcp2116-active-rs485-v1.1"
    test_id: str = "lcp_active_rs485"
    device_type: str = "LCP2116"
    result: TestStatus
    started_at: datetime
    duration_ms: int
    station_name: str
    serial_number: str
    operator: str
    port: str
    interfaces: list[ActiveInterfaceResult] = Field(default_factory=list)
    field_before_raw: str | None = None
    field_after_raw: str | None = None
    x2x_before_raw: str | None = None
    x2x_after_raw: str | None = None
    note: str = (
        "Python сам владеет fixture COM-портами и отвечает как Modbus RTU slave. "
        "FIXTURE_ERROR означает проблему стенда до проверки DUT."
    )
    error: str | None = None
    report_file: str | None = None


class EthernetInterfaceResult(BaseModel):
    name: str
    target_ip: str
    source_ip: str | None = None
    status: TestStatus
    link_before: str | None = None
    link_after: str | None = None
    socket_connected: bool = False
    transaction_id: int | None = None
    unit_id: int = 1
    registers: list[int] = Field(default_factory=list)
    before_requests: int | None = None
    after_requests: int | None = None
    before_responses: int | None = None
    after_responses: int | None = None
    before_transport_errors: int | None = None
    after_transport_errors: int | None = None
    duration_ms: int = 0
    detail: str


class LcpActiveEthernetResult(BaseModel):
    utility_version: str = "0.8.0"
    test_profile_version: str = "lcp2116-active-ethernet-v1"
    test_id: str = "lcp_active_ethernet"
    device_type: str = "LCP2116"
    result: TestStatus
    started_at: datetime
    duration_ms: int
    station_name: str
    serial_number: str
    operator: str
    port: str
    interfaces: list[EthernetInterfaceResult] = Field(default_factory=list)
    eth_before_raw: str | None = None
    eth_after_raw: str | None = None
    note: str = (
        "FIXTURE_ERROR означает отсутствие link, маршрута или доступного сетевого интерфейса ПК. "
        "FAIL используется только после установленного TCP-соединения и некорректного ответа DUT."
    )
    error: str | None = None
    report_file: str | None = None


class ActiveStepResult(BaseModel):
    name: str
    status: TestStatus
    duration_ms: int
    detail: str
    checks: list[CheckResult] = Field(default_factory=list)
    raw_before: str | None = None
    raw_action: str | None = None
    raw_after: str | None = None


class LcpActiveServicesResult(BaseModel):
    utility_version: str = "0.8.0"
    test_profile_version: str = "lcp2116-active-services-v1"
    test_id: str = "lcp_active_services"
    device_type: str = "LCP2116"
    result: TestStatus
    started_at: datetime
    duration_ms: int
    station_name: str
    serial_number: str
    operator: str
    port: str
    steps: list[ActiveStepResult] = Field(default_factory=list)
    note: str = (
        "Тест безопасно выполняет GET_CONFIG/VALIDATE_CONFIG без записи Flash, "
        "SDTEST.TXT write/read, синхронизацию RTC и короткую RTOS stability-проверку."
    )
    error: str | None = None
    report_file: str | None = None


class LcpHmiResult(BaseModel):
    utility_version: str = "0.8.0"
    test_profile_version: str = "lcp2116-hmi-echo-v1"
    test_id: str = "lcp_hmi_echo"
    device_type: str = "LCP2116"
    result: TestStatus
    started_at: datetime
    duration_ms: int
    station_name: str
    serial_number: str
    operator: str
    port: str
    endpoint: str | None = None
    serial: str = "9600 8N1"
    frames_sent: int = 0
    frames_received: int = 0
    expected_frames_hex: list[str] = Field(default_factory=list)
    actual_frames_hex: list[str] = Field(default_factory=list)
    detail: str
    error: str | None = None
    report_file: str | None = None


class LcpFlashAbResult(BaseModel):
    utility_version: str = "0.8.0"
    test_profile_version: str = "lcp2116-flash-ab-restore-v1"
    test_id: str = "lcp_flash_ab_restore"
    device_type: str = "LCP2116"
    result: TestStatus
    started_at: datetime
    duration_ms: int
    station_name: str
    serial_number: str
    operator: str
    port: str
    reconnected_port: str | None = None
    backup_file: str | None = None
    original_slot: int | None = None
    test_slot: int | None = None
    restored_slot: int | None = None
    original_crc32: str | None = None
    test_crc32: str | None = None
    restored: bool = False
    recovery_required: bool = False
    steps: list[ActiveStepResult] = Field(default_factory=list)
    note: str = (
        "Тест временно изменяет S4 baudrate в конфигурационном bundle, проверяет A/B commit "
        "после reboot и автоматически восстанавливает исходный bundle."
    )
    error: str | None = None
    report_file: str | None = None


class LcpWatchdogResetResult(BaseModel):
    utility_version: str = "0.8.0"
    test_profile_version: str = "lcp2116-watchdog-reset-v1"
    test_id: str = "lcp_watchdog_reset"
    device_type: str = "LCP2116"
    result: TestStatus
    started_at: datetime
    duration_ms: int
    station_name: str
    serial_number: str
    operator: str
    port: str
    reconnected_port: str | None = None
    before_boot_count: int | None = None
    after_boot_count: int | None = None
    reset_type: str | None = None
    recovery_performed: str | None = None
    steps: list[ActiveStepResult] = Field(default_factory=list)
    before_raw: str | None = None
    arm_raw: str | None = None
    after_raw: str | None = None
    error: str | None = None
    report_file: str | None = None


class LcpRtcRetentionResult(BaseModel):
    utility_version: str = "0.8.0"
    test_profile_version: str = "lcp2116-rtc-retention-v1"
    test_id: str = "lcp_rtc_retention"
    device_type: str = "LCP2116"
    phase: str
    result: TestStatus
    started_at: datetime
    duration_ms: int
    station_name: str
    serial_number: str
    operator: str
    port: str
    state_file: str | None = None
    prepared_at: datetime | None = None
    elapsed_pc_seconds: float | None = None
    elapsed_rtc_seconds: float | None = None
    retention_error_seconds: float | None = None
    before_boot_count: int | None = None
    after_boot_count: int | None = None
    battery_state: str | None = None
    steps: list[ActiveStepResult] = Field(default_factory=list)
    note: str = (
        "PREPARE синхронизирует RTC и сохраняет исходное состояние. VERIFY после полного "
        "отключения USB и основного питания проверяет ход RTC и увеличение boot_count."
    )
    error: str | None = None
    report_file: str | None = None
