#!/usr/bin/env bash
set -euo pipefail
cc="${1:-gcc}"
mkdir -p research-results
"$cc" -std=c11 -O3 -DNDEBUG -Wall -Wextra -Werror -o /tmp/matchlen-baseline tests/corefoundry_matchlen_gate.c lib/lz4.c
"$cc" -std=c11 -O3 -DNDEBUG -Wall -Wextra -Werror -DCOREF_MATCHLEN_SHORT -o /tmp/matchlen-candidate tests/corefoundry_matchlen_gate.c lib/lz4.c
/tmp/matchlen-baseline > research-results/baseline-1.txt
/tmp/matchlen-candidate > research-results/candidate-1.txt
/tmp/matchlen-candidate > research-results/candidate-2.txt
/tmp/matchlen-baseline > research-results/baseline-2.txt
python3 - <<'PY'
import re
from pathlib import Path
from statistics import median
pattern=re.compile(r"^CASE size=(\d+) kind=(\w+) bytes=(\d+) hash=(\d+) throughput_mibps=([\d.]+)$")
results={}
for name in ("baseline-1","baseline-2","candidate-1","candidate-2"):
    lines=Path(f"research-results/{name}.txt").read_text().splitlines()
    rows={}
    for line in lines:
        m=pattern.match(line)
        assert m,(name,line)
        size,kind,bytes_,digest,throughput=m.groups()
        rows[(int(size),kind)]=(int(bytes_),digest,float(throughput))
    assert len(rows)==18,(name,len(rows))
    results[name]=rows
ratios=[]
for case in sorted(results['baseline-1']):
    a,b,c,d=(results[k][case] for k in ("baseline-1","baseline-2","candidate-1","candidate-2"))
    assert len({(z[0],z[1]) for z in (a,b,c,d)})==1,(case,a,b,c,d)
    baseline=median((a[2],b[2]))
    candidate=median((c[2],d[2]))
    ratio=candidate/baseline
    ratios.append(ratio)
    print(f"CASE_RESULT n={case[0]} kind={case[1]} bytes={a[0]} baseline={baseline:.3f} candidate={candidate:.3f} speedup={ratio:.4f}x identical=YES")
mid=median(ratios)
gains=sum(x>1.01 for x in ratios)
losses=sum(x<0.98 for x in ratios)
print(f"GATE cases={len(ratios)} all_byte_identical=YES median_speedup={mid:.4f}x over_one_pct={gains} regress_over_two_pct={losses} worst={min(ratios):.4f}x")
print("DECISION=GO_CANDIDATE" if mid>=1.01 and losses==0 and gains>=9 else "DECISION=NO_GO")
PY
