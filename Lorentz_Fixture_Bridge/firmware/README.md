# Fixture Firmware Source

This directory will contain the buildable fixture firmware derived from the current `00_LCP2116/RTOS` source tree.

The production DUT source is not modified in place. The first import commit must preserve a clean build before DUT-specific services are replaced.

Current contents define only dependency-free bridge contracts:

- channel identifiers and fixed TCP port map;
- channel lifecycle states;
- serial configuration;
- transport/error counters.

Hardware and RTOS bindings are added only after the exact current firmware tree and build entry point are imported.
