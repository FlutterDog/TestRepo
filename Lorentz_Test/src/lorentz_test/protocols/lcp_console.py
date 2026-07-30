"""Text diagnostic console client for LCP Basic Firmware v1.02.0."""

from __future__ import annotations

import dataclasses
import re
import time
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


class LcpDiagnosticConsole:
    """Run short commands on an already opened LCP USB CDC transport."""

    def __init__(
        self,
        transport: SerialPort,
        *,
        command_timeout_seconds: float = 2.0,
        quiet_seconds: float = 0.15,
        session_settle_seconds: float = 0.05,
        maximum_response_bytes: int = 8192,
    ) -> None:
        self.transport = transport
        self.command_timeout_seconds = command_timeout_seconds
        self.quiet_seconds = quiet_seconds
        self.session_settle_seconds = session_settle_seconds
        self.maximum_response_bytes = maximum_response_bytes

    def execute(self, command: str) -> str:
        normalized = command.strip()
        if not normalized:
            raise ValueError("diagnostic command must not be empty")
        if len(normalized) > 63:
            raise ValueError("diagnostic command exceeds firmware limit of 63 characters")

        time.sleep(self.session_settle_seconds)
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

        if not output:
            raise DiagnosticConsoleError(
                f"no response to diagnostic command {normalized!r}"
            )

        return output.decode("utf-8", errors="replace").replace("\x00", "")

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
