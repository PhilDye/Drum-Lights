#!/usr/bin/env python3
"""Check that the RX and TX firmware (and the TX web UI) agree on the RF protocol.

Checks:
  1. The nRF24 pipe address is identical in RX and TX.
  2. Every mode the TX UI can send, and every mode in the TX auto list,
     has a matching `case` in the RX mode switch.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RX_MAIN = ROOT / "RX/src/main.cpp"
TX_MAIN = ROOT / "TX/src/main.cpp"
UI_HTML = ROOT / "TX/app/src/index.html"

errors = []


def pipe_address(path):
    m = re.search(r"const\s+byte\s+address\[5\]\s*=\s*\{([^}]*)\}", path.read_text())
    if not m:
        errors.append(f"{path.relative_to(ROOT)}: pipe address definition not found")
        return None
    return re.findall(r"'(.)'", m.group(1))


def rx_modes():
    return {int(n) for n in re.findall(r"^\s*case\s+(-?\d+)\s*:", RX_MAIN.read_text(), re.M)}


def tx_auto_modes():
    m = re.search(r"autoModes\[\d+\]\s*=\s*\{([^}]*)\}", TX_MAIN.read_text())
    if not m:
        errors.append("TX/src/main.cpp: autoModes list not found")
        return set()
    return {int(n) for n in re.findall(r"-?\d+", m.group(1))}


def ui_modes():
    return {int(n) for n in re.findall(r'data-mode="(-?\d+)"', UI_HTML.read_text())}


rx_addr, tx_addr = pipe_address(RX_MAIN), pipe_address(TX_MAIN)
if rx_addr and tx_addr and rx_addr != tx_addr:
    errors.append(f"pipe address mismatch: RX={''.join(rx_addr)} TX={''.join(tx_addr)}")

handled = rx_modes()
if not handled:
    errors.append("RX/src/main.cpp: no mode cases found")

# -1 is AUTO, handled on the TX and never sent over RF
sent = (ui_modes() | tx_auto_modes()) - {-1}
for mode in sorted(sent - handled):
    errors.append(f"mode {mode} is sent by the TX (UI or auto list) but has no case in the RX switch")

if errors:
    print("Protocol sync check FAILED:")
    for e in errors:
        print(f"  - {e}")
    sys.exit(1)

print(f"Protocol sync OK: address {''.join(rx_addr)}, {len(sent)} TX modes all handled by RX")
