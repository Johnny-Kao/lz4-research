#!/usr/bin/env bash
set -euo pipefail
cc="${1:-gcc}"
mkdir -p research-results
sources="tests/corefoundry_zero_write_probe.c lib/lz4file.c lib/lz4frame.c lib/lz4hc.c lib/lz4.c lib/xxhash.c"
"$cc" -std=c11 -O3 -DNDEBUG -Wall -Wextra -Werror -o /tmp/cf-zero-baseline $sources
"$cc" -std=c11 -O3 -DNDEBUG -Wall -Wextra -Werror -DCOREF_SKIP_ZERO_WRITE -o /tmp/cf-zero-candidate $sources
/tmp/cf-zero-baseline > research-results/b1.txt
/tmp/cf-zero-candidate > research-results/c1.txt
/tmp/cf-zero-candidate > research-results/c2.txt
/tmp/cf-zero-baseline > research-results/b2.txt
python3 - <<'PY'
import re
from pathlib import Path
from statistics import median
rx=re.compile(r"^CASE autoflush=(\d+) kind=(\w+) chunk=(\d+) loops=(\d+) bytes=(\d+) hash=(\d+) ns_per_write=([\d.]+)$")
runs={}
for name in ("b1","b2","c1","c2"):
    rows={}
    for line in Path(f"research-results/{name}.txt").read_text().splitlines():
        m=rx.match(line);assert m,(name,line)
        auto,kind,chunk,loops,bytes_,hash_,ns=m.groups()
        rows[(int(auto),kind,int(chunk))]=(int(loops),int(bytes_),hash_,float(ns))
    assert len(rows)==20,(name,len(rows))
    runs[name]=rows
ratios=[]
wins=0
for key in sorted(runs["b1"]):
    samples=[runs[k][key] for k in ("b1","b2","c1","c2")]
    assert len({s[:3] for s in samples})==1,(key,samples)
    base=median((samples[0][3],samples[1][3]))
    cand=median((samples[2][3],samples[3][3]))
    speedup=base/cand
    if key[0]==0 and key[2]<=512:
        ratios.append(speedup)
        wins+=speedup>1.05
    print(f"CASE_RESULT autoflush={key[0]} kind={key[1]} chunk={key[2]} base_ns={base:.2f} cand_ns={cand:.2f} speedup={speedup:.4f}x identical=YES")
median_fast=median(ratios)
print(f"GATE focused_cases={len(ratios)} over_five_pct={wins} median_speedup={median_fast:.4f}x exact_file_output=YES")
print("DECISION=GO_CANDIDATE" if wins>=4 and median_fast>=1.05 else "DECISION=NO_GO")
PY
