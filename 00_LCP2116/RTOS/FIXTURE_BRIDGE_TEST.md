# Lorentz Fixture Bridge 0.1.0-dev — S1 proof

## Source baseline

```text
base commit: be2071b2e307f4d76bafe63cc59804982a7552d8
branch: feature/lorentz-fixture-bridge-fw
project: 00_LCP2116/RTOS/LCP_Basic.cppproj
```

The Microchip Studio project file is unchanged for the first proof. `main.cpp` includes the fixture implementation directly, so the existing build list remains valid.

## Build target

Build `Release` and confirm:

```text
Build succeeded
0 compiler errors
0 compiler warnings
```

The output artifact still has the baseline project filename during this proof. The USB identity, not the artifact filename, distinguishes fixture firmware.

Expected USB command `version`:

```text
name = Lorentz Fixture Bridge Firmware
version = 0.1.0-dev
stage = S1 TCP bridge proof
target = ATSAM3X8E
product = LCP2116-FIXTURE
```

## Fixture network

```text
Fixture ETH1 IP: 192.168.1.200
Subnet:          255.255.255.0
Gateway:         192.168.1.254
TCP S1:          2101
```

Connect the PC and fixture ETH1 through the intended unmanaged switch. A direct cable is also acceptable for the first proof.

Recommended PC adapter setting:

```text
IP:      192.168.1.100
Subnet:  255.255.255.0
Gateway: empty
```

Check:

```powershell
Test-NetConnection 192.168.1.200 -Port 2101
```

## Serial wiring

Connect fixture S1 to DUT S1 using the same RS-485 wiring and termination used with the current USB-RS485 adapter.

```text
Fixture S1 A/B/GND <-> DUT S1 A/B/GND
```

DUT remains connected to the PC by USB. Do not replace the DUT management USB channel.

## Python station setting

In Lorentz Test station settings:

```text
S1 endpoint = tcp://192.168.1.200:2101
S2 endpoint = empty
S3 endpoint = empty
S4 endpoint = empty
```

Other currently proven endpoints may remain connected, but the first fixture acceptance should run S1 separately to isolate the bridge.

## Acceptance sequence

1. Flash the fixture LCP with `0.1.0-dev`.
2. Open fixture USB and run `version`.
3. Run `status`; confirm:
   - `ethernet = ready`;
   - `link = up`;
   - `S1 present = yes`;
   - `S1 tcp state = LISTENING`.
4. From Windows run `Test-NetConnection` for TCP 2101.
5. In Lorentz Test run only `Active S1-S4 and X2X` with only S1 configured.
6. Confirm DUT S1 receives the expected unique values and reports PASS.
7. Run fixture USB command `status` again.

Expected counters after one active test:

```text
tcp connections     > 0
tcp rx bytes         > 0
tcp tx bytes         > 0
uart rx bytes        > 0
uart tx bytes        > 0
tcp errors           = 0
uart write errors    = 0
tcp->uart overflow   = 0
uart->tcp overflow   = 0
uart hardware errors = 0
```

## Reconnect test

Repeat the active S1 test three times without rebooting the fixture. Each run must establish a new TCP connection and finish without stale bytes or manual recovery.

## Stop conditions

Do not expand to S2-S4/HMI/X2X if any of these occur:

- fixture USB identity is not distinct from DUT firmware;
- W5500 version/init fails;
- TCP 2101 does not return to LISTENING after disconnect;
- Modbus CRC errors appear in the Python report;
- bytes are lost or duplicated;
- any fixture ring-buffer overflow occurs;
- S1 requires a fixture reboot between runs.

After this proof passes, the W5500 HAL will be generalized from socket 0 to sockets 0-5 and the same bridge state machine will be instantiated for all six physical serial channels.
