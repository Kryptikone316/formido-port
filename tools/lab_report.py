#!/usr/bin/env python3
"""Compare labelled bot batches: tools/lab_report.py label [label ...] (reads botruns/sweep/<label>.txt)."""
import re, sys, statistics as st
def load(lab):
    rs = []
    try:
        for l in open(f"botruns/sweep/{lab}.txt"):
            if l.startswith("RESULT"):
                d = dict(kv.split("=", 1) for kv in l.split()[1:])
                rs.append({k: (int(v) if re.fullmatch(r"-?\d+", v) else v) for k, v in d.items()})
    except FileNotFoundError:
        pass
    return rs
def pct(v, p):
    v = sorted(v); return v[min(len(v) - 1, int(len(v) * p))]
for lab in sys.argv[1:]:
    rs = load(lab)
    if not rs:
        print(f"{lab:36s} (no results)"); continue
    secs = [r["secs"] for r in rs]; k = [r["kills"] for r in rs]
    h = [r.get("healed", 0) for r in rs]; ps = [r.get("platform_secs", 0) for r in rs]
    print(f"{lab:36s} n={len(rs):3d} median {st.median(secs):4.0f}s mean {st.mean(secs):5.0f}s p25-p75 {pct(secs,.25)}-{pct(secs,.75)} p90 {pct(secs,.9)}  kills med {st.median(k):4.0f}  >=5min {100*sum(1 for x in secs if x>=300)/len(rs):3.0f}%  Tyrant {100*sum(1 for r in rs if r['tyrants_seen']>0)/len(rs):3.0f}%  healed/game {st.mean(h):5.0f}  platform s/game {st.mean(ps):4.0f}")
