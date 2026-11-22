#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Summarise bot runs: tools/bot_stats.py botruns/b1.txt botruns/b2.txt ... (RESULT lines from run_bots.sh)."""
import re
import sys
import statistics as st

rows = []
for f in sys.argv[1:]:
    for line in open(f):
        if line.startswith("RESULT"):
            d = dict(kv.split("=", 1) for kv in line.split()[1:])
            d = {k: (int(v) if re.fullmatch(r"-?\d+", v) else v) for k, v in d.items()}
            d["file"] = f
            rows.append(d)


def pct(v, p):
    v = sorted(v)
    return v[min(len(v) - 1, int(len(v) * p))]


def line(name, rs):
    secs = [r["secs"] for r in rs]
    kills = [r["kills"] for r in rs]
    score = [r["score"] for r in rs]
    return (f"{name:14s} n={len(rs):3d}  survived: mean {st.mean(secs):5.1f}s  median {st.median(secs):5.1f}s  "
            f"p10 {pct(secs, .1):3d}s  p90 {pct(secs, .9):3d}s  max {max(secs):3d}s   "
            f"kills: mean {st.mean(kills):5.1f}  median {st.median(kills):5.1f}  p90 {pct(kills, .9):3d}  max {max(kills):3d}   "
            f"score mean {st.mean(score):5.1f}")


print(line("ALL", rows))
for sk in sorted({r["skill"] for r in rows}):
    print(line(f"skill {sk}", [r for r in rows if r["skill"] == sk]))
print()
for f in sorted({r["file"] for r in rows}):
    print(line(f.split("/")[-1], [r for r in rows if r["file"] == f]))
print()
print("kills by rank (mean per run): " + "  ".join(
    f"{k} {st.mean([r[k] for r in rows]):.1f}" for k in ("drone", "runner", "striker", "brute", "burrower", "stunner", "phantom", "tyrant")))
print(f"peak bugs alive: mean {st.mean([r['peak_bugs'] for r in rows]):.0f}  max {max(r['peak_bugs'] for r in rows)}")
print(f"runs that saw a Hive Tyrant: {sum(1 for r in rows if r['tyrants_seen'] > 0)}  slain: {sum(1 for r in rows if r['tyrants_slain'] > 0)}")
print(f"runs capped at 2 h: {sum(1 for r in rows if r['capped'] == 1)}")
print(f"runs longer than 1 min: {sum(1 for r in rows if r['secs'] > 60)}   2 min: {sum(1 for r in rows if r['secs'] > 120)}   "
      f"3 min: {sum(1 for r in rows if r['secs'] > 180)}   5 min: {sum(1 for r in rows if r['secs'] > 300)}")
secs = sorted(r["secs"] for r in rows)
print("survival curve (share of runs still alive): " + "  ".join(f"{t}s {100 * sum(1 for x in secs if x > t) / len(secs):.0f}%" for t in (30, 45, 60, 75, 90, 120, 180)))
