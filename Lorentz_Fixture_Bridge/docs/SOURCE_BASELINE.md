# Exact Source Baseline

The buildable firmware source is fixed at:

```text
commit: be2071b2e307f4d76bafe63cc59804982a7552d8
release: LCP Basic Diagnostic Firmware 1.02.0
project: 00_LCP2116/RTOS/LCP_Basic.cppproj
```

The fixture implementation must be developed from this commit, not from the repository's moving `main` branch. The source baseline contains the verified USB configuration transport, FreeRTOS application, board mappings, UART/SC16IS drivers, W5500 HAL, watchdog and diagnostics.

A dedicated immutable branch `firmware/lcp-basic-v1.02.0` points to that commit. Actual fixture firmware development uses a branch created from the same baseline so all source and project files are present.
