"""Text diagnostic console client for LCP Basic Firmware v1.02.0."""

from __future__ import annotations

import dataclasses
import re
import time
from collections.abc import Callable
from typing import Any

from lorentz_test.protocols.lcp_usb import SerialPort

_KEY_VALUE_LINE = re.compile(r"^\s*([A-Za-z0-9_.-]+)\s*=\s*(.*?)\s*$")


class DiagnosticConsoleError(RuntimeError):
    """The LCP text console did not return a complete valid response."""


@dataclasses.dataclass(frozen=True)
class FirmwareIdentity:
    name: str
    version: str
    stage: str
    target: str
    raw_output: str

    def to_dict(self) -> dict[str, str]:
        return dataclasses.asdict(self)


def parse_key_value_output(text: str) -> dict[str, str]:
    """Extract unordered ``key = value`` pairs from diagnostic output."""
    values: dict[str, str] = {}
    for line in text.splitlines():
        match = _KEY_VALUE_LINE.match(line)
        if match is not None:
            values[match.group(1).casefold()] = match.group(2).strip()
    return values


def _open_pyserial(port: str, baudrate: int, timeout: float) -> SerialPort:
    try:
        import serial

        return serial.Serial(
            port=port,
            baudrate=baudrate,
            timeout=timeout,
            write_timeout=timeout,
        )
    except Exception as exc:
        raise DiagnosticConsoleError(f"cannot open diagnostic console on {port}: {exc}") from exc


SerialFactory = Callable[[str, int, float], SerialPort]


class LcpDiagnosticConsole:
    """Run short commands on an LCP USB CDC text console."""

    def __init__(
        self,
        transport: SerialPort,
        *,
        command_timeout_seconds: float = 2.0,
        quiet_seconds: float = 0.15,
        session_settle_seconds: float = 0.10,
        command_attempts: int = 3,
        retry_delay_seconds: float = 0.25,
        maximum_response_bytes: int = 8192,
        owns_transport: bool = False,
    ) -> None:
        if command_attempts < 1:
            raise ValueError("command_attempts must be at least 1")
        self.transport = transport
        self.command_timeout_seconds = command_timeout_seconds
        self.quiet_seconds = quiet_seconds
        self.session_settle_seconds = session_settle_seconds
        self.command_attempts = command_attempts
        self.retry_delay_seconds = retry_delay_seconds
        self.maximum_response_bytes = maximum_response_bytes
        self.owns_transport = owns_transport

    @classmethod
    def open_port(
        cls,
        port: str,
        *,
        baudrate: int = 115200,
        timeout: float = 2.0,
        open_retry_seconds: float = 8.0,
        open_settle_seconds: float = 0.50,
        retry_delay_seconds: float = 0.25,
        serial_factory: SerialFactory | None = None,
    ) -> "LcpDiagnosticConsole":
        """Open CDC again after the binary session has released the port."""
        factory = serial_factory or _open_pyserial
        deadline = time.monotonic() + open_retry_seconds
        last_error: Exception | None = None

        while time.monotonic() < deadline:
            transport: SerialPort | None = None
            try:
                transport = factory(port, baudrate, timeout)
                time.sleep(open_settle_seconds)
                transport.reset_input_buffer()
                transport.reset_output_buffer()
                return cls(
                    transport,
                    command_timeout_seconds=timeout,
                    retry_delay_seconds=retry_delay_seconds,
                    owns_transport=True,
                )
            except Exception as exc:
                last_error = exc
                if transport is not None:
                    try:
                        if transport.is_open:
                            transport.close()
                    except Exception:
                        pass
                time.sleep(retry_delay_seconds)

        raise DiagnosticConsoleError(
            f"cannot reopen {port} for diagnostic console: {last_error or 'open timeout'}"
        ) from last_error

    def close(self) -> None:
        if not self.owns_transport:
            return
        try:
            if self.transport.is_open:
                self.transport.close()
        except Exception:
            pass

    def _execute_once(self, normalized: str) -> bytes:
        self.transport.reset_input_buffer()

        try:
            self.transport.write((normalized + "\r\n").encode("ascii"))
            self.transport.flush()
        except Exception as exc:
            raise DiagnosticConsoleError(f"console write failed: {exc}") from exc

        original_timeout: Any = getattr(self.transport, "timeout", None)
        timeout_changed = hasattr(self.transport, "timeout")
        if timeout_changed:
            try:
                setattr(self.transport, "timeout", min(0.05, self.command_timeout_seconds))
            except Exception:
                timeout_changed = False

        output = bytearray()
        deadline = time.monotonic() + self.command_timeout_seconds
        last_data_at: float | None = None

        try:
            while time.monotonic() < deadline:
                try:
                    chunk = self.transport.read(256)
                except Exception as exc:
                    raise DiagnosticConsoleError(f"console read failed: {exc}") from exc

                now = time.monotonic()
                if chunk:
                    output.extend(chunk)
                    last_data_at = now
                    if len(output) > self.maximum_response_bytes:
                        raise DiagnosticConsoleError(
                            f"console response exceeds {self.maximum_response_bytes} bytes"
                        )
                    continue

                if output and last_data_at is not None:
                    if (now - last_data_at) >= self.quiet_seconds:
                        break
        finally:
            if timeout_changed:
                try:
                    setattr(self.transport, "timeout", original_timeout)
                except Exception:
                    pass

        return bytes(output)

    def execute(self, command: str) -> str:
        normalized = command.strip()
        if not normalized:
            raise ValueError("diagnostic command must not be empty")
        if len(normalized) > 63:
            raise ValueError("diagnostic command exceeds firmware limit of 63 characters")

        time.sleep(self.session_settle_seconds)
        for attempt in range(1, self.command_attempts + 1):
            output = self._execute_once(normalized)
            if output:
                return output.decode("utf-8", errors="replace").replace("\x00", "")
            if attempt < self.command_attempts:
                time.sleep(self.retry_delay_seconds)

        raise DiagnosticConsoleError(
            f"no response to diagnostic command {normalized!r} after "
            f"{self.command_attempts} attempts"
        )

    def read_firmware_identity(self) -> FirmwareIdentity:
        raw_output = self.execute("version")
        values = parse_key_value_output(raw_output)
        required = ("name", "version", "stage", "target")
        missing = [key for key in required if not values.get(key)]
        if missing:
            raise DiagnosticConsoleError(
                "version response is missing keys: " + ", ".join(missing)
            )
        return FirmwareIdentity(
            name=values["name"],
            version=values["version"],
            stage=values["stage"],
            target=values["target"],
            raw_output=raw_output,
        )