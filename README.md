PSDR
====

PortableSDR - A Stand Alone HF Software Defined Transciever.

> Original hardware and firmware by Michael R. Colton (see `LICENSE`,
> `Documentation/` and `PSDR2 Guide.pdf`). This repository carries a
> headless Zephyr port of that design.


Zephyr Port
===========

The active work lives on the **`zephyr-port`** branch under
[`zephyr-port/`](zephyr-port/): a headless Zephyr RTOS port of the
PortableSDR-F7 board (ST **STM32F756VGTx**, 216 MHz). The original
display-driven Eclipse/OpenOCD firmware in `Source/` is kept only for
reference.


Build
-----

Builds run in the Colima `zephyr-build` container — no host toolchain
needed:

    ./zephyr-port/build.sh

This wraps `west build -b psdr_f7` against a west workspace
(`$ZEPHYR_WORKSPACE`, default `~/projects/zephyrproject`) and produces
`build/zephyr/zephyr.elf`.


Flash & watch
-------------

Flashing and console access go over a **Black Magic Probe** on SWD —
no DFU, no bootloader handover:

    SWDIO -> PA13   SWCLK -> PA14   SRST -> NRST   GND -> GND

The board console is USART1 on **PB6 (TX) / PB7 (RX) @ 115200**, which
the BMP also bridges as a UART pass-through CDC port.

    ./zephyr-port/scripts/bmp-flash.sh            # load zephyr.elf via SWD
    ./zephyr-port/scripts/psdrun.py --flash       # flash, then stream the UART

`psdrun.py` auto-detects both BMP CDC interfaces (GDB server + UART)
and can stream forever, until a regex, or for a fixed number of seconds.


Current state
-------------

* **Phase 1** — board bring-up: clocks (HSE 26 MHz → 216 MHz),
  console UART and the two status LEDs.
* **Phase 2** — a polled I/Q sampler (`zephyr-port/src/main.c`): reads
  RX_I on ADC1/PA3 and RX_Q on ADC2/PA2, computes a windowed RMS per
  channel and prints a UART telemetry line every 500 ms.
* **CMSIS-DSP primitives** (`zephyr-port/src/dsp.c`): FFT/filter/RMS
  routines ported 1:1 from the original firmware, exercised at boot by
  a deterministic impulse→iFFT self-test.

The sampler and the DSP pipeline are not yet wired together.

Carried-over numerics — `dsp.c` keeps the original math 1:1 on purpose
so behaviour stays comparable, including known quirks:

* the complex-multiply imaginary term uses the original (incorrect)
  `si*ci + sr*cr` instead of `sr*ci + si*cr` (flagged `BUG-PORT` in
  `psdr_dsp_apply_filter`);
* `psdr_dsp_set_filter` builds a hard **brick-wall** frequency-domain
  mask (0/1 coefficients, no taper).

These are intentionally left as-is until verified on a signal
generator; flip/refine them then.
