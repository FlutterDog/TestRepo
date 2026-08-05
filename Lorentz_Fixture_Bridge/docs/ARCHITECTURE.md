# Fixture Bridge Architecture

## Purpose

The fixture LCP2116 replaces the external multi-port serial server and USB-RS485 adapters. It is a transport device only.

The existing Lorentz Test application remains responsible for:

- opening the configured fixture endpoints;
- hosting Modbus RTU slaves and the X2X/HMI test behavior;
- generating test vectors;
- evaluating responses;
- producing JSON, HTML and PDF reports;
- deciding PASS, FAIL, FIXTURE_ERROR and INCOMPLETE.

The fixture firmware is responsible only for moving bytes between TCP and the physical serial ports.

## Data path

```text
Python RtuSlaveServer
    |
    | raw TCP stream
    v
Fixture W5500 socket
    |
    | bounded RX/TX ring buffers
    v
Physical UART / SC16IS channel
    |
    | RS-485 transceiver
    v
DUT serial port
```

The reverse direction uses the same channel in full duplex at the software level. Physical half-duplex direction is handled by the fixture UART driver.

## Ethernet topology

A small unmanaged five-port switch connects:

1. Windows PC;
2. fixture LCP2116;
3. DUT ETH1;
4. DUT ETH2;
5. one spare service port.

The DUT USB remains connected directly to the PC as an out-of-band management and diagnostics channel. It is intentionally not replaced by Ethernet.

## Socket allocation

One W5500 provides eight hardware sockets. The first fixture release uses six sockets:

- socket 0: S1 / TCP 2101;
- socket 1: S2 / TCP 2102;
- socket 2: S3 / TCP 2103;
- socket 3: S4 / TCP 2104;
- socket 4: HMI / TCP 2105;
- socket 5: X2X / TCP 2106.

Sockets 6 and 7 remain reserved for diagnostics or future use. USB is the primary management channel, so no management TCP socket is required in v0.1.

## Channel behavior

Each channel has independent state:

```text
DISABLED
LISTENING
CONNECTED
FAULT
```

Each channel owns:

- one TCP listening socket;
- one accepted client connection;
- one physical UART endpoint;
- one TCP-to-UART ring buffer;
- one UART-to-TCP ring buffer;
- UART format configuration;
- byte and error counters;
- last activity timestamps.

Only one TCP client is allowed per channel. A second connection is rejected or immediately closed. This prevents two applications from driving one DUT port.

## Transport rules

The bridge is protocol-transparent:

- it does not parse Modbus, X2X or HMI frames;
- it does not alter CRC values;
- it does not insert delimiters;
- it does not combine channels;
- it does not generate responses;
- it does not decide whether data is valid.

TCP is a stream, not a frame transport. The fixture forwards received bytes promptly and does not wait for a protocol-specific packet boundary.

## Latency and buffering

Initial targets:

- forwarding service period: <= 1 ms;
- no intentional coalescing delay;
- `TCP_NODELAY` equivalent behavior where supported by W5500 implementation;
- minimum 512-byte RX and TX ring buffer per channel;
- bounded buffers with explicit overflow counters;
- no blocking wait in the global service loop.

The exact buffer size will be validated against ATSAM3X8E SRAM usage after source integration.

## RS-485 direction control

For each half-duplex channel:

1. enable transmitter;
2. write buffered bytes to UART;
3. wait for the final stop bit / TX empty condition;
4. disable transmitter;
5. return to receive mode.

A fixed arbitrary delay is not sufficient. Direction release must follow the actual UART shift-register-empty condition or the proven timing mechanism already used in the current LCP firmware.

## Fault handling

A channel fault must not stop other channels. Faults include:

- UART overrun, framing or parity error;
- SC16IS communication error;
- TCP socket reset;
- ring-buffer overflow;
- repeated W5500 socket recovery;
- invalid channel configuration.

Recovery is local to the affected channel. The fixture watchdog remains global.

## Configuration

The first hardware defaults are stored in firmware. USB management commands can later change and persist:

- baudrate;
- parity;
- stop bits;
- enabled state;
- fixture IP, subnet and gateway.

The bridge TCP port numbers remain fixed to simplify station configuration and reporting.

## Version separation

Fixture identity must be distinct from DUT identity:

```text
name    = Lorentz Fixture Bridge Firmware
version = 0.1.0-dev
product = LCP2116-FIXTURE
```

The production firmware remains:

```text
name    = LCP Basic Diagnostic Firmware
version = 1.02.0
```
