# Lorentz Test 0.8.0 RC

Modular LCP2116 test suite for LCP Basic Firmware 1.02.0.

## New safe modules

- strict active ETH1/ETH2 Modbus TCP FC03 test;
- optional PC source-IP binding;
- safe `GET_CONFIG -> VALIDATE_CONFIG -> GET_CONFIG` test without Flash write;
- microSD `SDTEST.TXT` write/read check;
- RTC synchronization and tick check;
- short RTOS progression/reserve check;
- HMI exact echo test.

## New confirmed modules

- Flash A/B alternate-slot commit, reboot, exact read-back and automatic original-bundle restore;
- recovery JSON written before the first Flash mutation;
- watchdog hardware reset, CDC reconnect and firmware recovery verification;
- two-phase RTC battery-retention workflow with a complete manual power removal.

## Confirmation strings

- `FLASH A/B`
- `WATCHDOG RESET`

RTC retention uses explicit PREPARE and VERIFY buttons and sends internal phase confirmations automatically.

## Validation status

The implementation contains unit-test doubles for Modbus TCP, Ethernet orchestration, HMI echo, safe services, Flash bundle/writer helpers and destructive-test confirmation guards. A real Windows `pytest` run and hardware execution are still required before tagging or merging to `main`.
