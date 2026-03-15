The Atmel | SMART SAM3X/A series is a member of a family of Flash
microcontrollers based on the high performance 32-bit ARM Cortex-M3 RISC
processor. It operates at a maximum speed of 84 MHz and features up to
512 Kbytes of Flash and up to 100 Kbytes of SRAM. The peripheral set includes a
High Speed USB Host and Device port with embedded transceiver, an Ethernet
MAC, 2 CANs, a High Speed MCI for SDIO/SD/MMC, an External Bus Interface
with NAND Flash Controller (NFC), 5 UARTs, 2 TWIs, 4 SPIs, as well as a PWM
timer, three 3-channel general-purpose 32-bit timers, a low-power RTC, a low
power RTT, 256-bit General Purpose Backup Registers, a 12-bit ADC and a 12-bit
DAC.
The SAM3X/A devices have three software-selectable low-power modes: Sleep,
Wait and Backup. In Sleep mode, the processor is stopped while all other
functions can be kept running. In Wait mode, all clocks and functions are stopped
but some peripherals can be configured to wake up the system based on
predefined conditions. In Backup mode, only the RTC, RTT, and wake-up logic
are running.
The SAM3X/A series is ready for capacitive touch thanks to the QTouch library,
offering an easy way to implement buttons, wheels and sliders.
The SAM3X/A architecture is specifically designed to sustain high-speed data
transfers. It includes a multi-layer bus matrix as well as multiple SRAM banks,
PDC and DMA channels that enable it to run tasks in parallel and maximize data
throughput.
The device operates from 1.62V to 3.6V and is available in 100 and 144-lead
LQFP, 100-ball TFBGA and 144-ball LFBGA packages.
The SAM3X/A devices are particularly well suited for networking applications:
industrial and home/building automation, gateways

Features
 Core
̶
ARM Cortex-M3 revision 2.0 running at up to 84 MHz
̶
Memory Protection Unit (MPU)
̶
Thumb-2 instruction set
̶
24-bit SysTick Counter
̶
Nested Vector Interrupt Controller
 Memories
̶
256 to 512 Kbytes embedded Flash, 128-bit wide access, memory accelerator, dual bank
̶
32 to 100 Kbytes embedded SRAM with dual banks
̶
16 Kbytes ROM with embedded bootloader routines (UART, USB) and IAP routines
̶
Static Memory Controller (SMC): SRAM, NOR, NAND support. NFC with 4 Kbyte RAM buffer and ECC
 System
̶
Embedded voltage regulator for single supply operation
̶
Power-on-Reset (POR), Brown-out Detector (BOD) and Watchdog for safe reset
̶
Quartz or ceramic resonator oscillators: 3 to 20 MHz main and optional low power 32.768 kHz for RTC or device 
clock
̶
High precision 8/12 MHz factory trimmed internal RC oscillator with 4 MHz default frequency for fast device 
startup
̶
Slow Clock Internal RC oscillator as permanent clock for device clock in low-power mode
̶
One PLL for device clock and one dedicated PLL for USB 2.0 High Speed Mini Host/Device
̶
Temperature Sensor
̶
Up to 17 peripheral DMA (PDC) channels and 6-channel central DMA plus dedicated DMA for High-Speed USB 
Mini Host/Device and Ethernet MAC
 Low-power Modes
̶
Sleep, Wait and Backup modes, down to 2.5 µA in Backup mode with RTC, RTT, and GPBR
 Peripherals
̶
USB 2.0 Device/Mini Host: 480 Mbps, 4 Kbyte FIFO, up to 10 bidirectional Endpoints, dedicated DMA
̶
Up to 4 USARTs (ISO7816, IrDA®, Flow Control, SPI, Manchester and LIN support) and one UART
̶
2 TWI (I2C compatible), up to 6 SPIs, 1 SSC (I2S), 1 HSMCI (SDIO/SD/MMC) with up to 2 slots
̶
9-channel 32-bit Timer Counter (TC) for capture, compare and PWM mode, Quadrature Decoder Logic and 2-bit 
Gray Up/Down Counter for Stepper Motor
̶
Up to 8-channel 16-bit PWM (PWMC) with Complementary Output, Fault Input, 12-bit Dead Time Generator 
Counter for Motor Control
̶
32-bit low-power Real-time Timer (RTT) and low-power Real-time Clock (RTC) with calendar and alarm features
̶
256-bit General Purpose Backup Registers (GPBR)
̶
16-channel 12-bit 1 msps ADC with differential input mode and programmable gain stage
̶
2-channel 12-bit 1 msps DAC
̶
Ethernet MAC 10/100 (EMAC) with dedicated DMA
̶
2 CAN Controllers with 8 Mailboxes
̶
True Random Number Generator (TRNG)
̶
Register Write Protection
 I/O
̶
Up to 103 I/O lines with external interrupt capability (edge or level sensitivity), debouncing, glitch filtering and on
die Series Resistor Termination
̶
Up to six 32-bit Parallel Input/Outputs (PIO)


Power Supplies
The SAM3X/A series product has several types of power supply pins:
 VDDCORE pins: Power the core, the embedded memories and the peripherals; voltage ranges from 1.62V 
to 1.95V.
 VDDIO pins: Power the peripherals I/O lines; voltage ranges from 1.62V to 3.6V.
 VDDIN pin: Powers the voltage regulator
 VDDOUT pin: Output of the voltage regulator
 VDDBU pin: Powers the Slow Clock oscillator and a part of the System Controller; voltage ranges from 
1.62V to 3.6V. VDDBU must be supplied before or at the same time as VDDIO and VDDCORE.
 VDDPLL pin: Powers the PLL A, UPLL and 3–20 MHz Oscillator; voltage ranges from 1.62V to 1.95V.
 VDDUTMI pin: Powers the UTMI+ interface; voltage ranges from 3.0V to 3.6V, 3.3V nominal.
 VDDANA pin: Powers the ADC and DAC cells; voltage ranges from 2.0V to 3.6V.
Ground pins GND are common to VDDCORE and VDDIO pins power supplies. 
Separated ground pins are provided for VDDBU, VDDPLL, VDDUTMI and VDDANA. These ground pins are
respectively GNDBU, GNDPLL, GNDUTMI and GNDANA


