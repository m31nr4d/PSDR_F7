#!/usr/bin/env bash
# Flash zephyr.elf onto the PSDR-F7 via Black Magic Probe over SWD.
#
# Same pattern as ~/projects/stm32f405_base_app/scripts/bmp-flash.sh:
# arm-none-eabi-gdb drives the BMP GDB server directly. No DFU dance,
# no bootloader handover — works even if the firmware is wedged.
#
# Wiring (BMP -> PSDR SWD header):
#   SWDIO  -> PA13
#   SWCLK  -> PA14
#   SRST   -> NRST
#   GND    -> GND
#
# Usage:
#   ./scripts/bmp-flash.sh                    # default elf path
#   ./scripts/bmp-flash.sh path/to/zephyr.elf
#
# Env overrides:
#   BMP_DEV       — BMP GDB-server CDC device (auto-detected by default)
#   FIRMWARE_ELF  — same as the positional arg

set -euo pipefail

ELF="${1:-${FIRMWARE_ELF:-$HOME/projects/zephyrproject/build/zephyr/zephyr.elf}}"

if [ ! -f "$ELF" ]; then
  echo "ELF not found: $ELF" >&2
  echo "Run ./build.sh first, or pass the path explicitly." >&2
  exit 1
fi

if ! command -v arm-none-eabi-gdb >/dev/null 2>&1; then
  echo "arm-none-eabi-gdb not on PATH (brew install arm-none-eabi-gdb)." >&2
  exit 1
fi

# Auto-detect BMP GDB port if BMP_DEV not set.
# On macOS the BMP enumerates as /dev/cu.usbmodem<serial>1 (GDB server)
# and /dev/cu.usbmodem<serial>3 (UART pass-through). The "1" suffix
# is what we want here.
if [ -z "${BMP_DEV:-}" ]; then
  BMP_DEV=$(ls /dev/cu.usbmodem*1 2>/dev/null | head -1)
  if [ -z "$BMP_DEV" ]; then
    echo "No BMP GDB device found at /dev/cu.usbmodem*1." >&2
    echo "Plug in the BMP, or set BMP_DEV explicitly." >&2
    exit 1
  fi
fi

echo "BMP GDB device: $BMP_DEV"
echo "Flashing $ELF via SWD..."

arm-none-eabi-gdb --batch -nx \
  -ex "target extended-remote $BMP_DEV" \
  -ex "monitor swdp_scan" \
  -ex "attach 1" \
  -ex "load $ELF" \
  -ex "compare-sections" \
  -ex "monitor reset" \
  -ex "kill"

echo "Flash complete."
