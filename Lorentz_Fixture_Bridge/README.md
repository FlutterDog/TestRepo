# Lorentz Fixture Bridge

Dedicated fixture firmware for using one LCP2116 as a transparent multi-port TCP-to-serial bridge for Lorentz production tests.

The fixture does not decide PASS or FAIL and does not emulate device behavior. The existing Python test application remains the orchestrator and continues to run the Modbus RTU/X2X/HMI fixture logic. The LCP fixture only transports raw bytes between six TCP sockets and the six physical serial interfaces.

## Target topology

```text
Windows PC
  |-- USB --> DUT LCP2116 management and diagnostics
  |-- Ethernet --> small switch
                    |-- Fixture LCP2116
                    |-- DUT ETH1
                    |-- DUT ETH2
```

The Python station endpoints become:

```text
S1  = tcp://<fixture-ip>:2101
S2  = tcp://<fixture-ip>:2102
S3  = tcp://<fixture-ip>:2103
S4  = tcp://<fixture-ip>:2104
HMI = tcp://<fixture-ip>:2105
X2X = tcp://<fixture-ip>:2106
```

The current Lorentz Test transport already accepts raw TCP serial-server endpoints through pySerial `socket://`, so the existing Modbus fixture services can remain unchanged.

## Firmware responsibilities

- one independent raw TCP server per physical serial channel;
- transparent full-duplex byte transfer;
- independent RX/TX buffers and counters per channel;
- correct RS-485 direction control;
- UART overrun/error detection;
- one active TCP client per bridge channel;
- USB management console for identity, configuration, counters and recovery;
- fixture watchdog and self-diagnostics;
- no protocol parsing and no test verdict logic.

## Initial TCP map

| Channel | TCP port | Physical interface |
|---|---:|---|
| S1 | 2101 | SC16IS external UART |
| S2 | 2102 | SAM3X built-in UART |
| S3 | 2103 | SAM3X built-in UART |
| S4 | 2104 | SAM3X built-in UART |
| HMI | 2105 | SC16IS external UART |
| X2X | 2106 | SAM3X built-in UART |

See `docs/ARCHITECTURE.md`, `docs/HARDWARE_MAPPING.md` and `docs/MANAGEMENT_PROTOCOL.md`.

## Source baseline

The fixture project is derived from the current LCP2116 RTOS firmware under `00_LCP2116/RTOS`, but it remains a separate firmware product. Production DUT firmware 1.02.0 is not modified.

## Development status

Architecture baseline created. Next step is importing the exact current RTOS source tree and replacing the DUT services with six bridge services while preserving the proven USB, RTOS, Ethernet, watchdog and diagnostic infrastructure.
