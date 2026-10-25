#!/usr/bin/env python3
"""Score a receiver's serial log from the TX RF test (BLE spike, #19).

The TX test (serial command 'r' on a spike build) sends RF_TEST_STEPS steps,
alternating the solid colours 1-8 with strobe 98, each retransmitted 5 times.
The RX prints "Radio RX data: <mode>" for every packet it reads, so:

- a missed *change* shows up as a step absent from the log
- a missed *packet* shows up as fewer than 5 lines for a step

Missed changes can be over-counted by one when a colour step is lost and the
two strobes around it arrive with 5 or fewer packets between them; packet loss
is the more sensitive number for comparing BLE on and off anyway.

Usage: rf_score.py rx1.log [rx2.log ...]
Capture a log with e.g. `pio device monitor > rx1.log` in RX/ during the test.
"""
import re
import sys

STEPS = 200  # RF_TEST_STEPS in TX/src/main.cpp
RETRANSMITS = 5  # RETRANSMITS in TX/src/main.cpp
STROBE = 98
TEST_MODES = set(range(1, 9)) | {STROBE}


def expected():
    return [STROBE if i % 2 else 1 + (i // 2) % 8 for i in range(STEPS)]


def observed_runs(path):
    """Collapse the RX log into [mode, packet count] runs of test modes."""
    runs = []
    with open(path, errors="replace") as f:
        for line in f:
            m = re.search(r"Radio RX data: (-?\d+)", line)
            if not m or int(m.group(1)) not in TEST_MODES:
                continue
            mode = int(m.group(1))
            if runs and runs[-1][0] == mode:
                runs[-1][1] += 1
            else:
                runs.append([mode, 1])
    # if a colour step is missed, the strobes either side merge into one run;
    # more packets than one step sends means it held several steps
    split = []
    for mode, count in runs:
        while count > RETRANSMITS:
            split.append([mode, RETRANSMITS])
            count -= RETRANSMITS
        split.append([mode, count])
    return split


def align(exp, runs):
    """Longest common subsequence of expected steps and observed runs."""
    obs = [r[0] for r in runs]
    n, m = len(exp), len(obs)
    dp = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(n - 1, -1, -1):
        for j in range(m - 1, -1, -1):
            dp[i][j] = dp[i + 1][j + 1] + 1 if exp[i] == obs[j] else max(dp[i + 1][j], dp[i][j + 1])
    i = j = 0
    matched = []
    while i < n and j < m:
        if exp[i] == obs[j]:
            matched.append(runs[j])
            i += 1
            j += 1
        elif dp[i + 1][j] >= dp[i][j + 1]:
            i += 1
        else:
            j += 1
    return matched


def main(paths):
    if not paths:
        sys.exit(__doc__)
    exp = expected()
    print(f"{'log':<24} {'changes':>9} {'missed':>7} {'packets':>11} {'pkt loss':>9}")
    for path in paths:
        matched = align(exp, observed_runs(path))
        packets = sum(c for _, c in matched)
        total = STEPS * RETRANSMITS
        print(
            f"{path:<24} {len(matched):>4}/{STEPS:<4} {STEPS - len(matched):>7} "
            f"{packets:>5}/{total:<5} {100 * (total - packets) / total:>8.1f}%"
        )


if __name__ == "__main__":
    main(sys.argv[1:])
