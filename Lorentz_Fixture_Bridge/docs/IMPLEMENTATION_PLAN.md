# Fixture Bridge Implementation Plan

## Phase 0 — source import audit

- locate the exact current `00_LCP2116/RTOS` buildable source;
- identify its build command and generated firmware artifact;
- record the firmware tag/commit used for LCP Basic Diagnostic Firmware 1.02.0;
- copy the source into `Lorentz_Fixture_Bridge/firmware` without modifying the production tree;
- change product identity to `LCP2116-FIXTURE` and version to `0.1.0-dev`;
- confirm a clean build before functional changes.

## Phase 1 — single-channel proof

Implement only S1:

```text
TCP 2101 <-> S1 physical RS-485
```

Acceptance criteria:

- fixture boots and reports distinct identity over USB;
- fixture Ethernet link and IP are stable;
- TCP 2101 accepts exactly one client;
- Python `RtuSlaveServer` connects through `tcp://fixture:2101`;
- DUT S1 receives the same test values as with the existing COM adapter;
- no CRC, UART or buffer errors after 1000 requests;
- TCP disconnect/reconnect does not require fixture reboot.

This phase proves the complete architecture before allocating all channels.

## Phase 2 — six independent bridges

Add S2, S3, S4, HMI and X2X using the fixed port map 2102-2106.

Acceptance criteria:

- six simultaneous TCP clients;
- no shared buffers or state between channels;
- one channel can reconnect while the others continue;
- per-channel UART format is respected;
- no cross-channel bytes;
- watchdog remains serviced under full traffic.

## Phase 3 — USB management and pre-check integration

Add structured fixture status to the Lorentz Test pre-check:

- fixture USB identity;
- fixture firmware version;
- bridge listener states;
- configured UART formats;
- unresolved fixture errors;
- Ethernet link and IP.

Station settings gain:

```text
fixture_management_port
fixture_ip
fixture_expected_firmware
```

The existing S1-S4/HMI/X2X endpoint fields continue to contain normal `tcp://` URLs.

## Phase 4 — durability

- one-hour six-channel traffic test;
- repeated TCP connect/disconnect cycles;
- fixture watchdog recovery;
- Ethernet cable removal/restoration;
- DUT reset while channels remain connected;
- ring-buffer overflow injection;
- SC16IS error injection where practical;
- power-cycle recovery with saved network configuration.

## Phase 5 — product integration

- show the fixture LCP and six bridge channels on the station SVG page;
- show LISTENING, CONNECTED, WARNING and FAULT states;
- include fixture firmware and counters in the FULL_TEST station snapshot;
- package the fixture firmware binary with release documentation;
- freeze a fixture firmware version for Lorentz Test 1.0.

## Deliberate exclusions from v0.1

- no Windows virtual COM driver;
- no test verdict logic in fixture firmware;
- no Modbus parsing in the fixture;
- no DUT firmware changes;
- no DUT USB-over-Ethernet replacement;
- no managed-switch or VLAN dependency;
- no bridge through the second W5500 until the six-channel serial path is proven.
