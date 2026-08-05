# LCP2116 Fixture Hardware Mapping

This mapping is taken from the validated LCP diagnostic firmware and the LCP2116 schematic. It is the starting point for fixture firmware integration.

## Serial channels

| Fixture channel | Existing firmware hardware name | Implementation class | Initial TCP port |
|---|---|---|---:|
| X2X | `Serial` | ATSAM3X8E built-in UART | 2106 |
| S2 | `Serial1` | ATSAM3X8E built-in UART | 2102 |
| S3 | `Serial3` | ATSAM3X8E built-in UART | 2103 |
| S4 | `Serial2` | ATSAM3X8E built-in UART | 2104 |
| HMI | SC16IS, chip select 5, channel A | external UART | 2105 |
| S1 | SC16IS, chip select 6, channel A | external UART | 2101 |

The existing SC16IS probe also reports a `PC` logical port on chip select 3, channel A. It is not required for the six-channel bridge and remains reserved.

## Ethernet

The board contains two W5500 Ethernet controllers on the shared SPI bus with independent chip-select, reset, interrupt, MAC and PHY paths.

Fixture v0.1 uses one W5500 for all six TCP serial channels. The second W5500 remains unused/reserved. The external switch carries PC, fixture, DUT ETH1 and DUT ETH2 traffic.

## USB

The native USB connector remains the fixture management and recovery interface. Bridge data does not use USB in v0.1.

## RS-485 control signals

The board schematic exposes separate serial pairs and RTS/direction-control signals. Exact GPIO/SC16IS direction-control implementation must be copied from the current `00_LCP2116/RTOS` source rather than recreated from assumptions.

The import audit must identify for every channel:

- TX/RX driver instance;
- DE/RE or RTS signal;
- active polarity;
- TX-complete detection;
- current UART error counter source;
- required transceiver turnaround delay;
- LED mapping, if used.

## Initial UART defaults

The validated DUT firmware currently reports:

| Channel | Default format |
|---|---|
| S1 | 9600 8N1 |
| S2 | 9600 8N1 |
| S3 | 1200 8N1 in the tested configuration |
| S4 | 9600 8N1 |
| HMI | 9600 8N1 |
| X2X | 9600 8N1 |

Fixture defaults must be configurable because future module profiles may use different rates and parity.

## Open source-integration checks

Before the first compilable firmware commit:

1. locate the exact current RTOS source directory and build entry point;
2. copy the proven W5500 initialization and socket-recovery layer;
3. copy built-in UART and SC16IS ownership code;
4. copy RS-485 direction handling;
5. preserve USB console, watchdog, RTOS diagnostics and boot identity;
6. remove or disable DUT-specific FieldSensor, X2X-master and Modbus-TCP-server tasks;
7. verify SRAM allocation for twelve ring buffers and six bridge task contexts.
