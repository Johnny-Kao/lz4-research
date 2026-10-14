#!/usr/bin/env python3
import csv
import pathlib
import sys

directory = pathlib.Path(sys.argv[1])

def rows(name):
    with (directory / name).open(newline="") as f:
        return list(csv.reader(f))

base = {(r[1], r[2]): r for r in rows("hc-base.csv")}
hc8 = {(r[1], r[2]): r for r in rows("hc8.csv")}
assert base.keys() == hc8.keys(), "HC keys changed"
print("=== HC 4-byte vs 8-byte backward comparisons ===")
for k in sorted(base):
    a, b = base[k], hc8[k]
    assert a[-1] == b[-1] == "PASS", ("roundtrip", k)
    assert a[4:] == b[4:], ("HC output changed", k, a, b)
    print(f"HC_RESULT corpus={k[0]} level={k[1]} baseline_us={float(a[3]):.3f} candidate_us={float(b[3]):.3f} gain_pct={(float(a[3])/float(b[3])-1)*100:+.2f} size={a[4]} output_identical=YES")
print("=== Ideal ORACLE bound (post hoc, impossible to select for free online) ===")
oracle = {}
for r in rows("oracle.csv"):
    assert r[-1] == "PASS"
    oracle.setdefault(r[1], {})[int(r[2])] = (float(r[3]), int(r[4]))
for kind, options in oracle.items():
    for bandwidth in (100, 1000):
        costs = {a: latency+size/bandwidth for a, (latency, size) in options.items()}
        best = min(costs, key=costs.get)
        print(f"ORACLE_BOUND corpus={kind} bandwidth_MBps={bandwidth} accel_best={best} ideal_gain_vs_fixed1_pct={(costs[1]/costs[best]-1)*100:+.2f} fixed1_us={costs[1]:.3f} ideal_us={costs[best]:.3f}")
print("=== Sparse delta standalone FORMAT study (not LZ4 compatible) ===")
for r in rows("delta.csv"):
    assert r[-1] == "PASS"
    kind, changes = r[1], int(r[2])
    base_ns, base_size, delta_ns, delta_size = float(r[3]), int(r[4]), float(r[5]), int(r[6])
    print(f"DELTA_RESULT corpus={kind} edits={changes} lz4_bytes={base_size} delta_bytes={delta_size} size_gain_pct={(1-delta_size/base_size)*100:+.2f} lz4_ns={base_ns:.1f} delta_ns={delta_ns:.1f} nonstandard=YES")
print("VALIDATION: all HC outputs byte-identical; all benchmark roundtrips passed.")
