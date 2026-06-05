#!/usr/bin/env python3
"""psdrun -- PSDR-F7 boot + watch, scriptable.

Same shape as ~/projects/zynqberry-nextpnr/portable-kit/scripts/zbrun.py
but simpler — no U-Boot, no bitstream. Workflow:

  1. wait for the BMP USB ports to appear (so you can start psdrun
     and *then* plug the board in)
  2. optionally `arm-none-eabi-gdb`-flash zephyr.elf via the BMP
     GDB-server port
  3. stream the UART pass-through port to stdout

Modes:

  (no option)        stream indefinitely (until Ctrl+C)
  --until REGEX      stream until REGEX appears (or --timeout)
  --seconds N        stream for exactly N seconds, then exit

Exit codes: 0 ok, 2 on --until / --seconds timeout, 1 on a setup error.

Examples:
  # flash the default build, then watch forever (Ctrl+C to stop)
  ./scripts/psdrun.py --flash

  # flash + capture the first 3 telemetry lines, then exit
  ./scripts/psdrun.py --flash --until 'rate=' --timeout 5

  # just watch what's already on the board
  ./scripts/psdrun.py --seconds 10

The BMP exposes two CDC interfaces. On macOS they show up as:
  /dev/cu.usbmodem<serial>1   GDB server (for flashing)
  /dev/cu.usbmodem<serial>3   UART pass-through (PSDR USART1 -> here)
psdrun auto-detects both; override with BMP_GDB / BMP_UART env vars
if you have multiple probes plugged in.
"""
from __future__ import annotations

import argparse
import glob
import os
import re
import subprocess
import sys
import time

DEFAULT_ELF = os.path.expanduser(
    "~/projects/zephyrproject/build/zephyr/zephyr.elf"
)
DEFAULT_BAUD = 115200


def find_bmp_port(suffix: str, timeout: float) -> str | None:
    """Return /dev/cu.usbmodem<serial><suffix>, polling for up to `timeout`s.

    Suffix is "1" for the GDB server, "3" for the UART pass-through.
    """
    pattern = f"/dev/cu.usbmodem*{suffix}"
    deadline = time.time() + timeout
    while time.time() < deadline:
        hits = sorted(glob.glob(pattern))
        if hits:
            return hits[0]
        time.sleep(0.25)
    return None


def flash(elf: str, gdb_port: str) -> None:
    """Drive arm-none-eabi-gdb to load the elf onto the target.

    Raises SystemExit on failure.
    """
    if not os.path.isfile(elf):
        sys.exit(f"!! elf not found: {elf}")

    print(f">> flashing {elf} via {gdb_port}", flush=True)
    cmd = [
        "arm-none-eabi-gdb", "--batch", "-nx",
        "-ex", f"target extended-remote {gdb_port}",
        "-ex", "monitor swdp_scan",
        "-ex", "attach 1",
        "-ex", f"load {elf}",
        "-ex", "compare-sections",
        "-ex", "monitor reset",
        "-ex", "kill",
    ]
    rc = subprocess.call(cmd)
    if rc != 0:
        sys.exit(f"!! gdb load failed (rc={rc})")
    print(">> flash complete", flush=True)


def stream(port: str, baud: int, until: str | None,
           timeout: float, seconds: float | None) -> int:
    """Echo `port` to stdout. Return 0 ok, 2 on timeout."""
    try:
        import serial  # pyserial
    except ImportError:
        sys.exit("!! pyserial not installed (pip install pyserial)")

    s = serial.Serial(port, baud, timeout=0.1)
    print(f">> streaming {port} @ {baud} baud", flush=True)
    if until:
        print(f">> exit on /{until}/ (timeout {timeout}s)", flush=True)
    elif seconds is not None:
        print(f">> exit after {seconds}s", flush=True)
    else:
        print(">> Ctrl+C to stop", flush=True)

    rx = re.compile(until) if until else None
    deadline_until = time.time() + timeout if until else None
    deadline_secs = time.time() + seconds if seconds is not None else None

    buf = b""
    try:
        while True:
            chunk = s.read(4096)
            if chunk:
                sys.stdout.buffer.write(chunk)
                sys.stdout.flush()
                if rx is not None:
                    buf = (buf + chunk)[-8192:]
                    if rx.search(buf.decode("utf-8", "replace")):
                        print("\n>> matched.", flush=True)
                        return 0

            if deadline_until is not None and time.time() > deadline_until:
                print(f"\n!! timeout waiting for /{until}/", flush=True)
                return 2
            if deadline_secs is not None and time.time() > deadline_secs:
                print("\n>> seconds elapsed.", flush=True)
                return 0
    except KeyboardInterrupt:
        print("\n>> interrupted.", flush=True)
        return 0
    finally:
        s.close()


def main() -> int:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("--flash", nargs="?", const=DEFAULT_ELF, default=None,
                   metavar="ELF",
                   help="flash this elf before streaming (default: latest build)")
    p.add_argument("--baud", type=int, default=DEFAULT_BAUD,
                   help=f"UART baud rate (default {DEFAULT_BAUD})")
    p.add_argument("--wait-board", type=float, default=30.0,
                   help="seconds to wait for BMP ports to appear (default 30)")

    mode = p.add_mutually_exclusive_group()
    mode.add_argument("--until", default=None, metavar="REGEX",
                      help="stream until REGEX appears in the UART output")
    mode.add_argument("--seconds", type=float, default=None, metavar="N",
                      help="stream for exactly N seconds, then exit")
    p.add_argument("--timeout", type=float, default=30.0,
                   help="timeout for --until (default 30s)")

    args = p.parse_args()

    gdb_port = os.environ.get("BMP_GDB")
    uart_port = os.environ.get("BMP_UART")

    if args.flash is not None and not gdb_port:
        print(f">> waiting for BMP GDB port (up to {args.wait_board}s)...",
              flush=True)
        gdb_port = find_bmp_port("1", args.wait_board)
        if not gdb_port:
            sys.exit("!! no BMP GDB port found at /dev/cu.usbmodem*1")

    if not uart_port:
        print(f">> waiting for BMP UART port (up to {args.wait_board}s)...",
              flush=True)
        uart_port = find_bmp_port("3", args.wait_board)
        if not uart_port:
            sys.exit("!! no BMP UART port found at /dev/cu.usbmodem*3")

    if args.flash is not None:
        flash(args.flash, gdb_port)

    return stream(uart_port, args.baud,
                  until=args.until,
                  timeout=args.timeout,
                  seconds=args.seconds)


if __name__ == "__main__":
    sys.exit(main())
