
# sam-lib

The `sam-lib` C library provides an API for controlling peripherals of
microcontrollers. Supported devices include microcontrollers from the
Microchip (Atmel) AT91 family, specifically SAM3N, SAM3S, SAM4N, and SAM4S
chips. Supported standard peripherals include SUPC, RSTC, WDT, PMC, EEFC,
UART, USART, PIO, TWI, ADC, DACC, SPI, TC, and various hardware components
connected to the microcontroller, such as buttons, LEDs, LEDUI, IO extenders
(shift registers), and more.

## Library Features

- Standardized API (designed for the AZTech framework).
- Implementation of low-level serial communication protocols.
- Designed for real-time applications with multitasking support
  (dependent on FreeRTOS).
- Efficient interrupt handling.
- Utilizes DMA where applicable.
- Instances of communication peripherals are represented as C structures
  with synchronous (blocking) `read()` and `write()` operations.
- Supports low-power modes of the microcontroller. Peripheral blocks are
  turned off when entering sleep mode.
