1 Features
• Meets or exceeds the requirements of the TIA/
EIA-485A standard
• Low quiescent power– 0.3-mA Active mode– 1-nA Shutdown mode
• 1/8 Unit load up to 256 nodes on a bus
• Bus-pin ESD protection up to 15 kV
• Industry-standard SN75176 footprint
• Fail-safe receiver (bus open, bus shorted,
bus idle)
• Glitch-free power-up and power-down bus inputs 
and outputs
2 Applications
• Energy meter networks
• Motor control
• Power inverters
• Industrial automation
• Building automation networks
• Battery-powered applications
• Telecommunications equipmen

3 Description
The SNx5HVD308xE are half-duplex transceivers 
designed for RS-485 data bus networks. Powered 
by a 5-V supply, they are fully compliant with TIA/
EIA-485A standard. With controlled transition times, 
these devices are suitable for transmitting data over 
long twisted-pair cables. The SN65HVD3082E and 
SN75HVD3082E are optimized for signaling rates 
up to 200 kbps. The SN65HVD3085E is suitable 
for data transmission up to 1 Mbps, whereas the 
SN65HVD3088E is suitable for applications that 
require signaling rates up to
20 Mbps.
These devices are designed to operate with very 
low supply current, typically 0.3 mA, exclusive of the 
load. When in the inactive-shutdown mode, the supply 
current drops to a few nanoamps, which makes these 
devices ideal for power-sensitive applications.
The wide common-mode range and high ESD 
protection levels of these devices, make them suitable 
for demanding applications such as energy meter 
networks, electrical inverters, status and command 
signals across telecom racks, as well as cabled 
chassis interconnects, and industrial automation 
networks where noise tolerance is essential. These 
devices match the industry-standard footprint of the 
SN75176 device. The power-on-reset circuits keep 
the outputs in a high-impedance state until the 
supply voltage has stabilized. A thermal-shutdown 
function protects the device from damage, due to 
system fault conditions. The SN75HVD3082E is 
characterized for operation from 0°C to 70°C and 
SN65HVD308xE is characterized for operation from 
40°C to 85°C air temperature. The D package version 
of the SN65HVD3082E has been characterized for 
operation from –40°C to 105°C.
8.3 Feature Description
The SNx5HVD308xE provides internal biasing of the receiver input thresholds for open-circuit, bus-idle, or 
short-circuit fail-safe conditions. It features a typical hysteresis of 30 mV in order to improve noise immunity. 
Internal ESD protection circuits protect the transceiver bus terminals against ±15-kV Human Body Model (HBM) 
electrostatic discharges.
The devices protect themselves against damage due to overtemperature conditions, through the use of a 
thermal shutdown feature. Thermal shutdown is entered at 165°C (nominal) and causes the device to enter a 
low-power state with high-impedance outputs	

 Receiver Fail-safe
The differential receiver is fail-safe to invalid bus states caused by:
• Open bus conditions, such as a disconnected connector
• Shorted bus conditions, such as cable damage shorting the twisted-pair together
• Idle bus conditions that occur when no driver on the bus is actively driving
In any of these cases, the differential receiver outputs a fail-safe logic High state, so that the output of the 
receiver is not indeterminate.
Receiver fail-safe is accomplished by offsetting the receiver thresholds, so that the input indeterminate range 
does not include zero volts differential. To comply with the RS-422 and RS-485 standards, the receiver output 
must output a High when the differential input VID is more positive than +200 mV, and must output a Low when 
the VID is more negative than –200 mV. The receiver parameters which determine the fail-safe performance are 
VIT+ and VIT– and VHYS. As seen in the Electrical Characteristics table, differential signals more negative than –200 mV will always cause a Low receiver output and differential signals more positive than +200 mV will always 
cause a High receiver output.
When the differential input signal is close to zero, it is still above the maximum VIT+ threshold, and the receiver 
output is High. Only when the differential input is more negative than VIT– will the receiver output transition to a 
Low state. The noise immunity of the receiver inputs during a bus fault condition, includes the receiver hysteresis 
value VHYS (the separation between VIT+ and VIT– ) as well as the value of VIT+
