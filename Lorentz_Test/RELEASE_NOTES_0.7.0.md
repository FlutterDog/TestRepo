# Lorentz Test 0.7.0

Modular functional-test expansion for LCP2116 with LCP Basic Firmware 1.02.0.

## Added

- strict Modbus TCP FC03 client with MBAP and PDU validation;
- independent active ETH1 and ETH2 tests;
- optional source-IP binding for multi-adapter Windows test stations;
- link/cable/route/firewall failures classified as `FIXTURE_ERROR`;
- malformed Modbus TCP response after connection classified as `FAIL`;
- safe binary `GET_CONFIG -> VALIDATE_CONFIG -> GET_CONFIG` test;
- active microSD `SDTEST.TXT` write/read verification;
- RTC synchronization from the PC, read-back and tick progression test;
- short RTOS tick/uptime/heap/stack stability test;
- active HMI echo test with three exact frames;
- local COM and `tcp://host:port` HMI fixture endpoints;
- separate atomic JSON reports for Ethernet, services and HMI;
- browser controls for all new modules.

## Safety boundaries

The active services test does **not** issue `PUT_CONFIG`, does not write configuration Flash and does not reboot the controller. The microSD command modifies only the firmware service file `SDTEST.TXT`. RTC synchronization intentionally changes the local RTC date and time.

## Hardware validation still required

- ETH1 and ETH2 with actual cables and PC adapter/source-IP settings;
- HMI echo through the actual fixture adapter;
- service test on the real LCP/SD/RTC;
- full pytest run on the Windows development checkout.

Flash A/B write/restore, battery-retention power cycling and watchdog-reset recovery remain separate later modules because they intentionally alter persistent state or reset/power the DUT.
