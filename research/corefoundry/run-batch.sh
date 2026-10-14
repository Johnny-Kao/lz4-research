#!/usr/bin/env bash
set -euo pipefail
cc="${1:-gcc}"
echo "RESEARCH_NOTE: synthetic opportunity study; not an upstream optimization claim"
echo "COMPILER=$cc"
"$cc" -std=c11 -D_POSIX_C_SOURCE=200809L -O3 -DNDEBUG -Wall -Wextra -o /tmp/corefoundry-adaptive tests/corefoundry_adaptive_probe.c lib/lz4.c -lm
/tmp/corefoundry-adaptive > /tmp/corefoundry-adaptive-results.txt
echo "PROBE_A_FIXED_ACCELERATION_AND_NAIVE_ADAPTIVE"
rg '^(DATA|size=)' /tmp/corefoundry-adaptive-results.txt || true
echo "PROBE_B_POST_HOC_ORACLE_UPPER_BOUND"
python3 - <<'PY'
import re
from collections import defaultdict
data=defaultdict(dict)
for line in open('/tmp/corefoundry-adaptive-results.txt'):
    m=re.match(r'size=(\d+) kind=([^ ]+) mode=([^ ]+) accel=\d+ hits=\d+ MiBps=([\d.]+) ratio=([\d.]+)',line)
    if m:data[(int(m[1]),m[2])][m[3]]=(float(m[4]),float(m[5]))
for (n,k),d in sorted(data.items()):
    base=d["fixed1"]
    for max_ratio_increase in (0.0,0.01):
        eligible=[(mode,v) for mode,v in d.items() if mode in ("fixed1","fixed4","fixed16") and v[1]<=base[1]*(1+max_ratio_increase)+0.00005]
        choice=max(eligible,key=lambda x:x[1][0])
        print(f"ORACLE size={n} kind={k} ratio_tolerance={max_ratio_increase:.2f} choose={choice[0]} speedup={choice[1][0]/base[0]:.3f}x ratio={choice[1][1]:.4f}")
print("ORACLE_LIMIT: retrospective per-input selection; not deployable and no predictor overhead")
PY
echo "PROBE_C_SPARSE_DIFFERENCE_SIZE_BOUND"
python3 - <<'PY'
import random
rng=random.Random(20261014)
for n in (4096,65536,262144):
    reference=bytes(rng.randrange(256) for _ in range(n))
    for changed_pct in (0,0.01,0.1,0.5,1.0):
        changed=int(n*changed_pct)
        positions=rng.sample(range(n),changed)
        mutable=bytearray(reference)
        for i in positions:mutable[i]^=1
        exact=sum(a!=b for a,b in zip(reference,mutable))
        # Simple bitmask+residuals; includes reference payload cost only if reference is not already available
        encoded=(n+7)//8+exact
        print(f"DELTA n={n} changed={exact} bitmask_plus_bytes={encoded} fraction={encoded/n:.4f} requires_shared_reference=YES")
print("DELTA_LIMIT: representation arithmetic only; not an LZ4-compatible codec, excludes reference transport/metadata")
PY
