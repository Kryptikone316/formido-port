#!/usr/bin/env python3
"""Summarise the knob sweep: reads botruns/sweep/<knob>=<value>.txt (and base.txt), prints tables, writes botruns/sweep.json."""
import glob
import json
import os
import re
import statistics as st

rows = {}
for f in sorted(glob.glob("botruns/sweep/*.txt")):
    label = os.path.basename(f)[:-4]
    if label.endswith("-min"):
        continue
    rs = []
    for line in open(f):
        if line.startswith("RESULT"):
            d = dict(kv.split("=", 1) for kv in line.split()[1:])
            rs.append({k: (int(v) if re.fullmatch(r"-?\d+", v) else v) for k, v in d.items()})
    if rs:
        rows[label] = rs


def pct(v, p):
    v = sorted(v)
    return v[min(len(v) - 1, int(len(v) * p))]


def summary(rs):
    secs = [r["secs"] for r in rs]
    kills = [r["kills"] for r in rs]
    return dict(n=len(rs), mean=st.mean(secs), median=st.median(secs), p25=pct(secs, .25), p75=pct(secs, .75), p90=pct(secs, .9),
                max=max(secs), kmean=st.mean(kills), kmedian=st.median(kills), kp90=pct(kills, .9),
                s120=100 * sum(1 for x in secs if x >= 120) / len(secs), s300=100 * sum(1 for x in secs if x >= 300) / len(secs),
                tyrant=100 * sum(1 for r in rs if r["tyrants_seen"] > 0) / len(rs),
                capped=sum(1 for r in rs if r["capped"] == 1), peak=st.mean(r["peak_bugs"] for r in rs))


base = summary(rows["base"]) if "base" in rows else None
knobs = {}
combos = {}
for label, rs in rows.items():
    if label == "base":
        continue
    if label.startswith("combo_"):
        combos[label] = summary(rs)
        continue
    k, v = label.split("=")
    knobs.setdefault(k, []).append((float(v), summary(rs)))

out = {"base": base, "knobs": {}, "combos": {}}
hdr = f"{'setting':26s} {'n':>3s} {'median s':>8s} {'mean s':>7s} {'p25-p75':>9s} {'p90':>5s} {'max':>5s} {'kills med':>9s} {'kills mean':>10s} {'>=2min%':>7s} {'>=5min%':>7s} {'tyrant%':>7s}"
print(hdr)


def line(name, s):
    return (f"{name:26s} {s['n']:3d} {s['median']:8.0f} {s['mean']:7.1f} {s['p25']:4d}-{s['p75']:<4d} {s['p90']:5d} {s['max']:5d} "
            f"{s['kmedian']:9.0f} {s['kmean']:10.1f} {s['s120']:7.0f} {s['s300']:7.0f} {s['tyrant']:7.0f}")


if base:
    print(line("base (defaults)", base))
for k in sorted(knobs):
    print("-" * len(hdr))
    out["knobs"][k] = []
    for v, s in sorted(knobs[k]):
        print(line(f"{k} = {v:g}", s))
        out["knobs"][k].append(dict(value=v, **s))
print("=" * len(hdr))
for name, sm in sorted(combos.items()):
    print(line(name[:26], sm))
    out["combos"][name] = sm
json.dump(out, open("botruns/sweep.json", "w"), indent=1)
