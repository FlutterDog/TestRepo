#!/usr/bin/env python3
"""Stable launcher for the Lorentz LCP2116 USB configuration tool.

Adds a short Windows USB CDC settle delay after opening the COM port and retries
HELLO when the first frame is lost during port activation. The protocol and all
configuration commands remain implemented by lcp_config_usb.py.
"""

from __future__ import annotations

import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import lcp_config_usb as core


_ORIGINAL_INIT = core.LcpUsbClient.__init__
_ORIGINAL_HELLO = core.LcpUsbClient.hello


def _stable_init(self: core.LcpUsbClient,
                 port: str,
                 baudrate: int = 115200,
                 timeout: float = 2.0) -> None:
    _ORIGINAL_INIT(self, port=port, baudrate=baudrate, timeout=timeout)

    # usbser.sys may report the CDC port open before both directions are ready.
    time.sleep(0.50)

    # Discard only unsolicited text printed while the port was opening. No
    # machine request has been transmitted yet at this point.
    self.serial.reset_input_buffer()


def _stable_hello(self: core.LcpUsbClient) -> dict[str, int]:
    last_error: Exception | None = None

    for attempt in range(3):
        try:
            return _ORIGINAL_HELLO(self)
        except core.ProtocolError as exc:
            last_error = exc

            if attempt >= 2:
                break

            self.serial.reset_input_buffer()
            time.sleep(0.20)

    assert last_error is not None
    raise last_error


core.LcpUsbClient.__init__ = _stable_init
core.LcpUsbClient.hello = _stable_hello


if __name__ == "__main__":
    raise SystemExit(core.main())
