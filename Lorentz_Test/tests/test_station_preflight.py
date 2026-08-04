import serial

from lorentz_test.models.tests import EndpointAccessStatus as Access
from lorentz_test.services.station_preflight import probe_endpoint_access


class FakeSerialHandle:
    def __init__(self) -> None:
        self.closed = False

    def close(self) -> None:
        self.closed = True


def test_available_serial_endpoint_is_opened_and_released() -> None:
    handle = FakeSerialHandle()

    def opener(**_: object) -> FakeSerialHandle:
        return handle

    result = probe_endpoint_access(
        "S1",
        "COM11",
        baudrate=115200,
        serial_opener=opener,
    )
    assert result.status == Access.AVAILABLE
    assert handle.closed is True
    assert "доступен" in result.detail


def test_busy_serial_endpoint_is_station_error() -> None:
    def opener(**_: object) -> object:
        raise serial.SerialException("could not open port 'COM11': PermissionError(13, 'Access is denied.')")

    result = probe_endpoint_access(
        "S1",
        "COM11",
        baudrate=115200,
        serial_opener=opener,
    )
    assert result.status == Access.BUSY
    assert "ошибка стенда" in result.detail
    assert "занят" in result.detail


def test_unconfigured_endpoint_is_skipped() -> None:
    result = probe_endpoint_access("S4", None, baudrate=115200)
    assert result.status == Access.SKIPPED


def test_unsupported_endpoint_format_is_reported() -> None:
    result = probe_endpoint_access("S1", "localhost:4001", baudrate=115200)
    assert result.status == Access.UNSUPPORTED
    assert "ошибка настройки стенда" in result.detail
