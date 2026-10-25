#!/usr/bin/env python3
"""Check that the RX and TX firmware use the same nRF24 pipe address.

The mode catalogue (TX/app/src/modes.json) is checked against both firmwares
by the web build (TX/app/catalogue-plugin.js), so only the address is checked here.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FILES = {"RX": ROOT / "RX/src/main.cpp", "TX": ROOT / "TX/src/main.cpp"}


def pipe_address(path):
    m = re.search(r"const\s+byte\s+address\[5\]\s*=\s*\{([^}]*)\}", path.read_text())
    return "".join(re.findall(r"'(.)'", m.group(1))) if m else None


addresses = {name: pipe_address(path) for name, path in FILES.items()}
missing = [name for name, addr in addresses.items() if not addr]
if missing:
    print(f"Protocol sync check FAILED: pipe address not found in {', '.join(missing)}")
    sys.exit(1)

if len(set(addresses.values())) != 1:
    print(f"Protocol sync check FAILED: pipe address mismatch {addresses}")
    sys.exit(1)

print(f"Protocol sync OK: pipe address {addresses['RX']}")
