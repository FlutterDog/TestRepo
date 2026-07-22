#!/usr/bin/env python3
"""Stable Windows launcher for the Lorentz LCP2116 USB configuration tool.

The protocol and configuration model remain implemented by lcp_config_usb.py.
This launcher adds Windows usbser.sys handling:

* retry opening a CDC port while an old handle is being removed;
* wait briefly for both CDC directions after opening;
* retry HELLO after a transient Windows serial error;
* after a configuration write and reboot, find the controller again, reconnect,
  read the active configuration, and verify it against the transmitted bundle.
"""

from __future__ import annotations

import pathlib
import sys
import time
from typing import Any

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import lcp_config_usb as core
from serial.tools import list_ports


_OPEN_RETRY_SECONDS = 8.0
_OPEN_SETTLE_SECONDS = 0.50
_RECONNECT_SECONDS = 25.0
_RETRY_DELAY_SECONDS = 0.25

_ORIGINAL_INIT = core.LcpUsbClient.__init__
_ORIGINAL_HELLO = core.LcpUsbClient.hello


def _raw_close(client: core.LcpUsbClient) -> None:
    serial_port = getattr(client, "serial", None)
    if serial_port is None:
        return
    try:
        if serial_port.is_open:
            serial_port.close()
    except Exception:
        pass


def _open_ready(client: core.LcpUsbClient,
                port: str,
                baudrate: int,
                timeout: float,
                deadline: float) -> None:
    last_error: Exception | None = None

    while time.monotonic() < deadline:
        try:
            _ORIGINAL_INIT(client, port=port, baudrate=baudrate, timeout=timeout)
            client._stable_port = port
            client._stable_baudrate = baudrate
            client._stable_timeout = timeout

            # usbser.sys may expose the COM name before both directions are ready.
            time.sleep(_OPEN_SETTLE_SECONDS)

            # No machine request has been sent yet, so it is safe to discard only
            # unsolicited text that accumulated while the port was opening.
            client.serial.reset_input_buffer()
            client.serial.reset_output_buffer()
            return
        except (core.serial.SerialException, OSError) as exc:
            last_error = exc
            _raw_close(client)
            time.sleep(_RETRY_DELAY_SECONDS)

    if last_error is not None:
        raise last_error
    raise core.ProtocolError(f"timeout opening {port}")


def _stable_init(self: core.LcpUsbClient,
                 port: str,
                 baudrate: int = 115200,
                 timeout: float = 2.0) -> None:
    _open_ready(
        self,
        port=port,
        baudrate=baudrate,
        timeout=timeout,
        deadline=time.monotonic() + _OPEN_RETRY_SECONDS,
    )


def _stable_hello(self: core.LcpUsbClient) -> dict[str, int]:
    last_error: Exception | None = None

    for attempt in range(3):
        try:
            return _ORIGINAL_HELLO(self)
        except (core.ProtocolError, core.serial.SerialException, OSError) as exc:
            last_error = exc

            if attempt >= 2:
                break

            port = self._stable_port
            baudrate = self._stable_baudrate
            timeout = self._stable_timeout
            _raw_close(self)
            time.sleep(_RETRY_DELAY_SECONDS)
            _open_ready(
                self,
                port=port,
                baudrate=baudrate,
                timeout=timeout,
                deadline=time.monotonic() + _OPEN_RETRY_SECONDS,
            )

    assert last_error is not None
    raise last_error


def _port_identity(port: str) -> tuple[int | None, int | None, str | None]:
    for item in list_ports.comports():
        if item.device.casefold() == port.casefold():
            return item.vid, item.pid, item.serial_number
    return None, None, None


def _candidate_ports(preferred: str,
                     identity: tuple[int | None, int | None, str | None]) -> list[str]:
    vid, pid, serial_number = identity
    result: list[str] = []

    for item in list_ports.comports():
        matches_identity = (
            vid is not None and
            pid is not None and
            item.vid == vid and
            item.pid == pid and
            (serial_number is None or item.serial_number == serial_number)
        )
        if item.device.casefold() == preferred.casefold() or matches_identity:
            if item.device not in result:
                result.append(item.device)

    return result


def _wait_for_reconnect(
        port: str,
        timeout: float,
        expected: core.LcpConfig,
        identity: tuple[int | None, int | None, str | None]) -> core.ActiveConfigInfo:
    expected_payload = expected.pack()
    deadline = time.monotonic() + _RECONNECT_SECONDS
    last_error: Exception | None = None

    while time.monotonic() < deadline:
        candidates = _candidate_ports(port, identity)

        for candidate in candidates:
            try:
                with core.LcpUsbClient(candidate, timeout=timeout) as client:
                    client.hello()
                    active = client.read_config()

                if active.config.pack() != expected_payload:
                    last_error = core.ProtocolError(
                        "controller reconnected but the active configuration is still old"
                    )
                    continue

                return active
            except (core.ProtocolError,
                    core.serial.SerialException,
                    OSError) as exc:
                last_error = exc

        time.sleep(_RETRY_DELAY_SECONDS)

    if last_error is not None:
        raise core.ProtocolError(
            f"controller did not reconnect with the written configuration: {last_error}"
        ) from last_error
    raise core.ProtocolError("controller did not reappear after reboot")


def _write_and_verify(args: Any, config: core.LcpConfig) -> None:
    # Preserve VID/PID/serial before REBOOT removes the current COM device.
    identity = _port_identity(args.port)

    with core.LcpUsbClient(args.port, timeout=args.timeout) as client:
        client.hello()
        result = client.write_config(config, reboot=not args.no_reboot)

    print(core.STATUS_TEXT[result])

    if result != core.STATUS_REBOOT_REQUIRED or args.no_reboot:
        return

    active = _wait_for_reconnect(
        args.port,
        args.timeout,
        config,
        identity,
    )
    print(
        "reconnected and verified: "
        f"source={active.source} slot={active.slot} "
        f"sequence={active.sequence} crc32=0x{active.crc32:08X}"
    )


def _stable_command_write(args: Any) -> None:
    _write_and_verify(args, core.load_json(args.input))


def _stable_command_write_txt(args: Any) -> None:
    _write_and_verify(args, core.load_legacy_txt(args.directory))


core.LcpUsbClient.__init__ = _stable_init
core.LcpUsbClient.hello = _stable_hello
core.command_write = _stable_command_write
core.command_write_txt = _stable_command_write_txt


if __name__ == "__main__":
    raise SystemExit(core.main())
