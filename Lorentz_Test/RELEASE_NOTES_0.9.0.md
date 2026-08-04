# Lorentz Test 0.9.0 development

Current build: backend `0.9.0.dev1`, frontend `0.9.0-p1`.

This branch develops the product layer above the hardware-validated 0.8.0 test core. Firmware remains 1.02.0.

## Implemented in dev1

### Station pre-check

A non-destructive readiness check is available through:

```text
POST /api/tests/lcp/precheck
```

It checks:

- LCP USB binary protocol and firmware identity;
- configured S1-S4, HMI and X2X endpoints for availability and ownership;
- enabled ETH1/ETH2 TCP port 502 reachability, including optional source-IP binding;
- reports and runtime directories for write access;
- consistency of the shared HMI/X2X adapter setting.

Blocking failures produce `NOT_READY`. Unconfigured optional interfaces and shared-adapter workflow notes produce warnings without blocking a partial stand. The full-test button automatically runs pre-check and does not create a backend run session until blocking issues are removed.

### Operator-oriented navigation

The previous single technical page is divided into:

- **Проверка** — operator identity, station pre-check and the one-click full test;
- **Настройка стенда** — COM endpoints, Ethernet addresses and fixture options;
- **Сервисные тесты** — individual diagnostics plus Flash A/B, watchdog and RTC retention.

The selected tab is preserved locally across page refreshes.

### Versioned reports

The JSON writer stamps every saved module and aggregate report with the actual running backend version. The 0.9 aggregate report identifies frontend build `0.9.0-p1` while keeping the validated hardware profile `lcp2116-full-sequential-v1.1` unchanged.

## Remaining 0.9 plan

1. Review and refine the operator/settings UI from real use.
2. Add report history and filtering.
3. Generate a structured printable HTML report.
4. Add PDF export from the approved HTML layout.
5. Package and verify the application on a clean Windows computer.

## Validation target

The 0.8.0 baseline contained 64 tests. Dev1 adds three station-precheck service tests, so the expected next Windows result is:

```text
67 passed, 1 warning
```

The external Starlette/httpx warning remains unchanged.
