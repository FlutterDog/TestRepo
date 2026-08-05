# Fixture USB Management Protocol

## Scope

USB is a management and recovery channel. It does not carry the normal S1-S4, HMI or X2X bridge traffic.

The first implementation may reuse the current text diagnostic console. A binary API can be added later only when the Python application needs structured configuration or counters.

## Identity

The `version` command must return a product identity that cannot be confused with a DUT:

```text
name = Lorentz Fixture Bridge Firmware
version = 0.1.0-dev
product = LCP2116-FIXTURE
target = ATSAM3X8E
```

## Required commands

### `fixture status`

Returns global fixture state:

- uptime;
- RTOS and heap status;
- fixture IP/MAC/link;
- active W5500 controller;
- watchdog state;
- number of listening and connected channels;
- aggregate UART, TCP and buffer errors.

### `bridge list`

Returns one line per channel:

```text
S1  tcp=2101 state=LISTENING serial=9600 8N1 rx=0 tx=0 errors=0
S2  tcp=2102 state=CONNECTED serial=9600 8N1 rx=120 tx=85 errors=0
```

### `bridge show CHANNEL`

Returns the complete configuration and counters for one channel.

### `bridge set CHANNEL BAUD PARITY STOPBITS`

Example:

```text
bridge set S3 1200 N 1
```

The first implementation may apply settings only until reboot. Persistent storage is a later step after the bridge path is stable.

### `bridge enable CHANNEL`

Starts or resumes the TCP listener and serial channel.

### `bridge disable CHANNEL`

Closes the TCP client/listener and releases the serial channel.

### `bridge reset CHANNEL`

Closes the active TCP connection, clears buffers, reinitializes the UART and returns the channel to LISTENING.

### `bridge reset all`

Performs the same recovery for all six channels without rebooting the fixture.

### `bridge counters clear [CHANNEL|all]`

Clears byte, connection and error counters.

### `network show`

Returns fixture MAC, IP, subnet, gateway, link and socket allocation.

### `network set IP SUBNET GATEWAY`

Configures the fixture management/bridge address. Persistence is deferred until basic operation is proven.

## Counter model

Per channel:

- `tcp_connections`;
- `tcp_disconnects`;
- `tcp_rx_bytes`;
- `tcp_tx_bytes`;
- `uart_rx_bytes`;
- `uart_tx_bytes`;
- `uart_overrun_errors`;
- `uart_framing_errors`;
- `uart_parity_errors`;
- `tcp_errors`;
- `rx_buffer_overflows`;
- `tx_buffer_overflows`;
- `last_activity_ms`;
- `last_error`.

## Python integration

The existing Lorentz Test application does not need management commands for the first bridge proof. It can use the fixed IP and TCP ports directly.

Later pre-check integration can identify the fixture through USB and verify:

- expected fixture firmware version;
- Ethernet link;
- all required bridge listeners;
- channel configuration;
- zero unresolved fixture faults.

A bridge failure remains a `FIXTURE_ERROR`, never a DUT `FAIL`.
