#!/usr/bin/env bash
set -euo pipefail
cc="${1:-gcc}"
mkdir -p research-results
"$cc" -std=c11 -O3 -DNDEBUG -Wall -Wextra -Werror -o /tmp/lz4-delta-one-shot tests/corefoundry_delta_vs_lz4.c lib/lz4.c
/tmp/lz4-delta-one-shot | tee research-results/delta-vs-lz4-raw.txt
python3 - <<'PY'
from pathlib import Path
from collections import defaultdict
import re
pat=re.compile(r"^CASE n=(\d+) ref=(\w+) edits=(\d+) pattern=(\w+) mode=(\w+) bytes=(\d+) encode_ns=(\d+) decode_ns=(\d+) checksum=(\d+) correctness=PASS$")
groups=defaultdict(dict)
for line in Path('research-results/delta-vs-lz4-raw.txt').read_text().splitlines():
    m=pat.match(line)
    assert m,line
    n,ref,edits,pattern,mode,byte_count,enc,dec,hashval=m.groups()
    key=(int(n),ref,int(edits),pattern)
    groups[key][mode]={'bytes':int(byte_count),'enc':int(enc),'dec':int(dec),'hash':hashval}
assert len(groups)==40,len(groups)
stats={name:{'smaller':0,'enc_faster':0,'dec_faster':0,'all_three':0,'biggest_size_saving':0,'worst_size_regression':0} for name in ('bitmap','positions')}
for key, modes in sorted(groups.items()):
    assert set(modes)=={'lz4dict','bitmap','positions'},(key,modes.keys())
    assert len({m['hash'] for m in modes.values()})==1,(key,modes)
    base=modes['lz4dict']
    line=[]
    for name in ('bitmap','positions'):
        candidate=modes[name]
        size_ratio=candidate['bytes']/base['bytes']
        encode_speedup=base['enc']/max(1,candidate['enc'])
        decode_speedup=base['dec']/max(1,candidate['dec'])
        s=stats[name]
        s['smaller']+=candidate['bytes']<base['bytes']
        s['enc_faster']+=candidate['enc']<base['enc']
        s['dec_faster']+=candidate['dec']<base['dec']
        s['all_three']+=candidate['bytes']<base['bytes'] and candidate['enc']<base['enc'] and candidate['dec']<base['dec']
        s['biggest_size_saving']=max(s['biggest_size_saving'],1-size_ratio)
        s['worst_size_regression']=max(s['worst_size_regression'],size_ratio-1)
        line.append(f"{name}:size={size_ratio:.3f}x enc_speed={encode_speedup:.2f}x dec_speed={decode_speedup:.2f}x")
    print(f"COMPARE size={key[0]} ref={key[1]} edits_per_mille={key[2]} pattern={key[3]} lz4_bytes={base['bytes']} {' '.join(line)}")
for name,s in stats.items():
    print(f"RESULT scheme={name} total={len(groups)} smaller={s['smaller']} encode_faster={s['enc_faster']} decode_faster={s['dec_faster']} all_three={s['all_three']} max_size_saving={s['biggest_size_saving']:.2%} worst_size_regression={s['worst_size_regression']:.2%} correctness=PASS")
print("UPSTREAM_GATE=NOT_ELIGIBLE_WITHOUT_FORMAT_CHANGE (new bitmap and positions representations cannot be consumed by the stock LZ4 decoder)")
PY
