# Lorentz Test 0.6.1

Hardware classification correction based on the first real LCP2116 active fixture report.

## Confirmed hardware result

The first 0.6.0 run confirmed USB identity and all passive diagnostics. The active report exposed a fixture cross-connection:

- the slave started on the endpoint configured as S1 received and answered requests;
- its unique S1 marker `0x1101/0x1102` appeared in the LCP S2 runtime;
- therefore that fixture endpoint was physically connected to S2, not S1;
- other configured field endpoints received no requests;
- the X2X endpoint was held by another Windows process.

This is not sufficient evidence of a DUT failure.

## Classification changes

The active test now classifies the following as `FIXTURE_ERROR`:

- a unique field marker appears on a different LCP S-port;
- an opened endpoint receives no requests from LCP;
- fixture serial I/O fails after the endpoint was opened;
- the fixture receives requests but does not send responses;
- X2X endpoint receives no polling traffic.

A cross-connection result explicitly identifies the endpoint, its configured S-port and the LCP S-port that actually received the marker.

`FAIL` is retained only when the fixture observed valid request traffic, sent responses, and no cross-port marker explains why the intended DUT channel did not accept the exchange.

## Report profile

- utility version: `0.6.1`
- active profile: `lcp2116-active-rs485-v1.1`

Final release tagging and merge to `main` remain blocked until the corrected fixture mapping and X2X endpoint pass a repeated active run.
