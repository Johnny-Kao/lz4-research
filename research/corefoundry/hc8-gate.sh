#!/usr/bin/env bash
set -euo pipefail
cc="${1:-gcc}"
mkdir -p research-results
"$cc" -std=c11 -O3 -DNDEBUG -Wall -Wextra -o /tmp/hc8-baseline tests/corefoundry_hc8_gate.c lib/lz4.c lib/lz4hc.c
"$cc" -std=c11 -O3 -DNDEBUG -Wall -Wextra -DCOREF_8BYTE_COUNTBACK -o /tmp/hc8-candidate tests/corefoundry_hc8_gate.c lib/lz4.c lib/lz4hc.c
/tmp/hc8-baseline > research-results/base1.log
/tmp/hc8-candidate > research-results/candidate1.log
/tmp/hc8-candidate > research-results/candidate2.log
/tmp/hc8-baseline > research-results/base2.log
python3 - <<'PY'
from pathlib import Path
from statistics import median
from collections import defaultdict
import re
pat=re.compile(r'^CASE n=(\d+) kind=(\w+) level=(\d+) bytes=(\d+) hash=(\d+) mibps=([\d.]+)$')
all_runs={}
for key in ('base1','candidate1','candidate2','base2'):
    rows={}
    for line in Path(f'research-results/{key}.log').read_text().splitlines():
        m=pat.match(line)
        assert m, (key,line)
        n,kind,level,num_bytes,hash_value,mbps=m.groups()
        case=(int(n),kind,int(level))
        rows[case]=(int(num_bytes),int(hash_value),float(mbps))
    assert len(rows)==16,(key,len(rows))
    all_runs[key]=rows
speedups=[]
for case in sorted(all_runs['base1']):
    data=[all_runs[k][case] for k in all_runs]
    assert len({(row[0],row[1]) for row in data})==1,(case,data)
    b=median([all_runs[k][case][2] for k in ('base1','base2')])
    c=median([all_runs[k][case][2] for k in ('candidate1','candidate2')])
    ratio=c/b
    speedups.append(ratio)
    print(f'CASE_RESULT size={case[0]} kind={case[1]} level={case[2]} baseline={b:.2f} candidate={c:.2f} speedup={ratio:.4f}x bytes={data[0][0]} output_identical=YES')
overall=median(speedups)
wins=sum(r>1.03 for r in speedups)
losses=sum(r<0.98 for r in speedups)
print(f'GATE total_cases={len(speedups)} output_identical=YES median_speedup={overall:.4f}x wins_over_3pct={wins} losses_over_2pct={losses} worst={min(speedups):.4f}x')
if wins>=8 and losses==0 and overall>1.03:
    print('DECISION=GO_CANDIDATE')
else:
    print('DECISION=NO_GO')
PY
