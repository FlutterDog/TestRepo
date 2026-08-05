# Local fixture proof without a second LCP

## Purpose

This procedure validates the first fixture channel without a DUT controller.
The existing four-port serial server acts as the physical peer for fixture S1.

```text
Windows PC
  |-- Ethernet --> Fixture LCP ETH1 (192.168.1.200:2101)
  |-- COM/TCP ----> Four-port serial server, one selected port

Fixture LCP S1 A/B/GND <---- RS-485 ----> Serial-server port A/B/GND
```

The PC opens both endpoints and sends deterministic binary payloads in both
directions. This validates the complete path:

```text
PC TCP
  -> fixture W5500
  -> fixture bridge firmware
  -> fixture S1 transceiver
  -> RS-485 cable
  -> serial-server transceiver
  -> PC peer endpoint
```

and the reverse path.

## Before flashing the local LCP

The local controller will temporarily stop being a DUT and become the fixture.
Save its active configuration before flashing:

```powershell
Set-Location D:\IGAS_Transfer\TestRepo\00_LCP2116\RTOS
New-Item -ItemType Directory -Force .\test_output | Out-Null
python .\tools\lcp_config_usb.py read `
    --port COM_DUT `
    --output .\test_output\local_lcp_config_before_fixture.json
```

Replace `COM_DUT` with the current USB COM port.

The fixture firmware does not intentionally write the configuration A/B area,
but the saved JSON is required because a programmer may perform a full-chip
erase depending on its settings.

## Firmware

Use branch:

```text
feature/lorentz-fixture-bridge-fw
```

Build and flash:

```text
00_LCP2116/RTOS/LCP_Basic.cppproj
Configuration: Release
```

Expected USB identity:

```text
name = Lorentz Fixture Bridge Firmware
version = 0.1.0-dev
stage = S1 TCP bridge proof
product = LCP2116-FIXTURE
```

## Network

Fixture ETH1 is fixed for this proof:

```text
IP:      192.168.1.200
Subnet:  255.255.255.0
TCP S1:  2101
```

Recommended PC adapter:

```text
IP:      192.168.1.100
Subnet:  255.255.255.0
Gateway: empty
```

Check before running the byte test:

```powershell
Test-NetConnection 192.168.1.200 -Port 2101
```

## Serial-server port

Configure one port as:

```text
Mode:       RS-485 2-wire / half duplex
Baudrate:   9600
Data bits:  8
Parity:     none
Stop bits:  1
Flow:       none
Local echo: off
```

The peer may be exposed to Windows either as a virtual COM port or as a raw TCP
serial-server socket. Both forms are supported by the verification script.

Examples:

```text
COM4
socket://192.168.1.50:4001
```

Wire:

```text
Fixture S1 A   <-> serial-server A
Fixture S1 B   <-> serial-server B
Fixture GND    <-> serial-server GND
```

Some vendors use opposite A/B naming. If neither direction transfers any bytes,
check the device documentation or swap A and B once. Do not swap repeatedly when
bytes already arrive with corruption; that indicates a different problem.

## Install Python dependency

The repository test environment already uses pySerial. For a standalone Python:

```powershell
python -m pip install pyserial
```

## Run with a virtual COM peer

Example fixture USB `COM10` and serial-server peer `COM4`:

```powershell
Set-Location D:\IGAS_Transfer\TestRepo\00_LCP2116\RTOS
python .\tools\fixture_bridge_verify.py `
    --fixture socket://192.168.1.200:2101 `
    --peer COM4 `
    --fixture-usb COM10 `
    --baud 9600 `
    --output .\test_output\fixture_bridge_local.json
```

## Run with a raw TCP peer

```powershell
python .\tools\fixture_bridge_verify.py `
    --fixture socket://192.168.1.200:2101 `
    --peer socket://192.168.1.50:4001 `
    --fixture-usb COM10 `
    --baud 9600 `
    --output .\test_output\fixture_bridge_local.json
```

## What the verifier does

- opens fixture TCP and peer endpoint simultaneously;
- sends fixed and seeded random binary payloads;
- checks exact bytes from fixture to peer;
- checks exact bytes from peer to fixture;
- rejects missing, changed, duplicated and extra bytes;
- closes and reopens fixture TCP three times;
- confirms transfer after each reconnect;
- optionally captures fixture USB `status` before and after;
- writes a JSON report and returns process exit code 0 only for complete PASS.

Default run contains more than forty bidirectional transfers plus three reconnect
cycles. It is intentionally a raw-byte test and does not depend on Modbus framing.

Expected console summary:

```text
Fixture bridge verification: PASS
Transfers: N/N PASS
Reconnects: 3/3 PASS
Report: ...fixture_bridge_local.json
```

## Acceptance criteria

The local proof is accepted when:

- all transfers pass in both directions;
- all three reconnect cycles pass;
- no changed or extra bytes are detected;
- fixture `status` reports non-zero TCP/UART byte counters;
- fixture reports zero ring-buffer overflow;
- fixture reports zero UART write and hardware errors;
- no fixture reboot is needed between runs.

Run the verifier three complete times without rebooting the fixture.

## What this proof does and does not establish

It establishes:

- fixture Ethernet and W5500 server behavior;
- transparent TCP-to-S1 and S1-to-TCP transport;
- physical RS-485 direction handling;
- binary transparency;
- reconnect recovery;
- compatibility with the existing serial-server infrastructure.

It does not yet establish:

- interaction with DUT LCP firmware;
- Modbus timing under the final DUT request cycle;
- S2, S3, S4, HMI or X2X channels;
- six simultaneous W5500 sockets.

After this local proof passes, the same fixture firmware is flashed to the remote
stand LCP and tested against a real DUT. Only then is the implementation expanded
to all six serial channels.

## Returning the local LCP to DUT firmware

After the proof, flash the verified LCP Basic 1.02.0 release again. Read the active
configuration through USB. If the programmer erased the reserved A/B area,
restore the saved JSON with the existing configuration utility and verify exact
read-back.
