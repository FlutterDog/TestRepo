ERASE button
The ERASE button is used to reinitialize the Flash content (and some of its NVM bits) to an erased state (all bits read
as logic level 1). The ERASE buttonand the ROM code ensure an in-situ reprogrammability of the Flash content
without the use of a debug tool. When the security bit is activated, the ERASE buttonprovides the capability to
reprogram the Flash content. It integrates a pull-down resistor of about 100 kΩ to GND, so that it can be left
unconnected for normal operations.
This pin is debounced by SCLK to improve the glitch tolerance. When the ERASE buttonis tied high during less than
100 ms, it is not taken into account. The pin must be tied high during more than 220 ms to perform a Flash erase
operation.
The ERASE buttonis a system I/O pin and can be used as a standard I/O. At startup, the ERASE buttonis not configured
as a PIO pin. If the ERASE buttonis used as a standard I/O, the startup level of this pin must be low to prevent
unwanted erasing. Please refer to Section 9.3 “Peripheral Signal Multiplexing on I/O Lines”. Also, if the ERASE pin
is used as a standard I/O output, asserting the pin to high does not erase the Flash.