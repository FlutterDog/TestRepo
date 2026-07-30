# Lorentz Test 0.6.0

Hardware release candidate for LCP2116 with LCP Basic Firmware 1.02.0.

## Active RS-485 fixture

The utility now owns the configured fixture endpoints and runs host-side Modbus RTU slaves. Modbus Poll is not required and must be closed during the active test.

### FieldSensor S1-S4

Each configured endpoint receives an independent slave address 1. The utility reads the actual serial format from the LCP `field` report and answers FC03 register 0, count 2 with a unique pattern:

- S1: `0x1101`, `0x1102`
- S2: `0x2201`, `0x2202`
- S3: `0x3301`, `0x3302`
- S4: `0x4401`, `0x4402`

PASS requires a request received by Python, a response sent, an increased LCP success counter, `connection=online`, `valid=yes`, `last_result=ok`, and exact read-back of the unique values.

### X2X

The first active X2X profile emulates one configured `LCT1114_2` slave. It supports the firmware 1.02.0 polling sequence:

- chunked FC03 reads of main registers `0..93`, maximum 16 registers per request;
- FC03 read of waveform flag register `850`;
- flag value `0`, so waveform transfer remains inactive.

PASS requires the expected request sequence, responses sent by Python, an increased LCP success counter, `connection=online`, `communication_error=ok`, and zero consecutive failures.

## Endpoint types

- local Windows serial port: `COM11`
- raw TCP serial server channel: `tcp://192.168.1.50:4001`

## Result classification

- `PASS`: the active end-to-end communication path passed;
- `FIXTURE_ERROR`: endpoint missing, busy, inaccessible, or Python could not start the slave;
- `FAIL`: Python owned the endpoint and ran the emulator, but LCP did not complete the expected exchange;
- `SKIPPED`: endpoint not configured or X2X module profile not supported yet.

## Reports

Active reports are written as:

```text
LCP2116_<serial>_<timestamp>_ACTIVE_RS485_<status>.json
```

They include before/after LCP diagnostics, fixture request counters, response counters, observed request addresses, expected values, and actual values.

## Validation status

The implementation includes unit tests for Modbus CRC16, FC03 responses, exceptions, active orchestration, passive-scope separation, fixture-error classification, and JSON report naming. Final release tagging and merge to `main` remain blocked until the real LCP and fixture adapters pass the active hardware run.
