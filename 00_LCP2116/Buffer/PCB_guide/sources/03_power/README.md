1 Features
• 3.5 to 28-V input voltage range
• Adjustable output voltage down to 0.8 V
• Integrated 80-mΩ high-side MOSFET supports up 
to 3-A continuous output current
• High efficiency at light loads with a pulse skipping 
Eco-mode
• Fixed 570-kHz switching frequency
• Typical 1-μA shutdown quiescent current
• Adjustable slow-start limits inrush currents
• Programmable UVLO threshold
• Overvoltage transient protection
• Cycle-by-cycle current limit, frequency foldback, 
and thermal shutdown protection
• Available in easy-to-use SOIC8 package 
or thermally-enhanced SOIC8 PowerPAD™ 
integrated circuit package
• Create a custom design using the TPS54331 with 
the WEBENCH® Power Designer
• Use TPS62933 for a 30 VIN converter with higher 
frequency, lower IQ and improved EMI
2 Applications
• Consumer applications such as set-top boxes, 
CPE equipment, LCD displays, peripherals, and 
battery chargers
• Industrial and car-audio power supplies
• 5-V, 12-V, and 24-V distributed power systems

3 Description
The TPS54331 device is a 28-V, 3-A non
synchronous buck converter that integrates a low 
RDS(on) high-side MOSFET. To increase efficiency 
at light loads, a pulse skipping Eco-mode feature 
is 
automatically activated. Furthermore, the 1-μA 
shutdown supply-current allows the device to be 
used in battery-powered applications. Current mode 
control with internal slope compensation simplifies 
the external compensation calculations and reduces 
component count while allowing the use of ceramic 
output capacitors. A resistor divider programs the 
hysteresis of the input undervoltage lockout. An 
overvoltage transient protection circuit limits voltage 
overshoots during start-up and transient conditions. 
A cycle-by-cycle current-limit scheme, frequency 
foldback and thermal shutdown protect the device 
and the load in the event of an overload condition. 
The TPS54331 device is available in an 8-pin SOIC 
package and 8-pin SO PowerPAD integrated circuit 
package that have been internally optimized to 
improve thermal performance.

The TPS54331 device is a 28-V, 3-A, step-down (buck) converter with an integrated high-side n-channel 
MOSFET. To improve performance during line and load transients, the device implements a constant-frequency 
current mode control, which reduces output capacitance and simplifies external frequency compensation design. 
The TPS54331 device has a preset switching frequency of 570 kHz.
The TPS54331 device requires a minimum input voltage of 3.5 V for normal operation. The EN pin has an 
internal pullup current source that can adjust the input-voltage undervoltage lockout (UVLO) with two external 
resistors. In addition, the pullup current provides a default condition when the EN pin is floating for the device 
to operate. The operating current is 110 μA (typical) when not switching and under no load. When the device is 
disabled, the supply current is 1 μA (typical).
The integrated 80-mΩ high-side MOSFET allows for high-efficiency power-supply designs with continuous 
output currents up to 3 A.
The TPS54331 device reduces the external component count by integrating the boot recharge diode. The bias 
voltage for the integrated high-side MOSFET is supplied by an external capacitor on the BOOT to PH pin. The 
boot capacitor voltage is monitored by an UVLO circuit and turns the high-side MOSFET off when the voltage 
falls below a preset threshold of 2.1 V (typical). The output voltage can be stepped down to as low as the 
reference voltage.
By adding an external capacitor, the slow-start time of the TPS54331 device can be adjustable, which enables 
flexible output filter selection.
To improve the efficiency at light load conditions, the TPS54331 device enters a special pulse skipping Eco
mode when the peak inductor current drops below 160 mA (typical).
The frequency foldback reduces the switching frequency during start-up and overcurrent conditions to help 
control the inductor current. The thermal shutdown provides additional protection under fault conditions

7.3.1 Fixed-Frequency PWM Control
The TPS54331 device uses a fixed-frequency, peak-current mode control. The internal switching frequency of 
the TPS54331 device is fixed at 570 kHz.
7.3.2 Voltage Reference (VREF)
The voltage reference system produces a ±2% initial accuracy voltage reference (±3.5% over temperature) by 
scaling the output of a temperature-stable band-gap circuit. The typical voltage reference is designed at 0.8 V.
7.3.3 Bootstrap Voltage (BOOT)
The TPS54331 device has an integrated boot regulator and requires a 0.1-μF ceramic capacitor between the 
BOOT and PH pins to provide the gate-drive voltage for the high-side MOSFET. A ceramic capacitor with 
an X7R- or X5R-grade dielectric is recommended because of the stable characteristics over temperature and 
voltage. To improve dropout, the TPS54331 device is designed to operate at 100% duty cycle as long as the 
BOOT-to-PH pin voltage is greater than 2.1 V (typical).
7.3.4 Enable and Adjustable Input Undervoltage Lockout (VIN UVLO)
The EN pin has an internal pullup current-source that provides the default condition of the device while operating 
when the EN pin floats.
The TPS54331 device is disabled when the VIN pin voltage falls below the internal VIN UVLO threshold. Using 
an external VIN UVLO to add hysteresis is recommended unless the VIN voltage is greater than (VOUT + 2 V).

7.3.9 Overcurrent Protection and Frequency Shift
The TPS54331 device implements current mode control that uses the COMP pin voltage to turn off the high-side 
MOSFET on a cycle-by-cycle basis. During each cycle, the switch current and the COMP pin voltage are 
compared. When the peak inductor current intersects the COMP pin voltage, the high-side switch is turned off. 
During overcurrent conditions that pull the output voltage low, the error amplifier responds by driving the COMP 
pin high, causing the switch current to increase. The COMP pin has a maximum clamp internally, which limits the 
output current.
The TPS54331 device provides robust protection during short circuits. Overcurrent runaway is possible in the 
output inductor during a short circuit at the output. The TPS54331 device solves this issue by increasing the off 
time during short-circuit conditions by lowering the switching frequency. The switching frequency is divided by 
1, 2, 4, and 8 as the voltage ramps from 0 to 0.8 V on VSENSE pin. The relationship between the switching 
frequency and the VSENSE pin voltage is listed in Table 7-2.
Table 7-2. Switching Frequency Conditions
Switching Frequency VSENSE Pin Voltage
570 kHz VSENSE ≥ 0.6 V
570 kHz / 2 0.6 V > VSENSE ≥ 0.4 V
570 kHz / 4 0.4 V > VSENSE ≥ 0.2 V
570 kHz / 8 0.2 V > VSENS


7.3.10 Overvoltage Transient Protection
The TPS54331 device incorporates an overvoltage transient-protection (OVTP) circuit to minimize output voltage 
overshoot when recovering from output fault conditions or strong unload transients. The OVTP circuit includes 
an overvoltage comparator to compare the VSENSE pin voltage and internal thresholds. When the VSENSE 
pin voltage goes above 109% × VREF, the high-side MOSFET is forced off. When the VSENSE pin voltage falls 
below 107% × VREF, the high-side MOSFET is enabled again.
7.3.11 Thermal Shutdown
The device implements an internal thermal shutdown to protect the device if the junction temperature exceeds 
165°C. The thermal shutdown forces the device to stop switching when the junction temperature exceeds the 
thermal trip threshold. When the die temperature decreases below 165°C, the device reinitiates the power-up 
sequence.