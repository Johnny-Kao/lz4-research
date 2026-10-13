# Dictionary copy instrumentation — 2026-10-14 JST

Independent Research repository only. Branch `research/dict-copy-probe`, parent `dba0f566`. Apple Silicon arm64, Apple Clang `-O3 -DNDEBUG`. **No upstream PR.**

## Research-only instrumentation
`lib/lz4frame.c` adds counters around `LZ4F_localSaveDict` for invocation count and total returned `dictSize` (proxy for bytes retained/moved; not measured memory traffic or exclusive memcpy timing). `tests/corefoundry_frame_probe.c` prints counters for linked-block `autoFlush=1` compression.

## Workload
1 MiB generated input; 600 repetitions = 600 MiB logical input per row, same compression context reused, chunk sizes 1/8/64 KiB, stableSrc 0 vs 1; roundtrip decompress and byte equality checked after last repetition. One run only, time estimates not statistically controlled.

| chunk | stableSrc | dict calls | sum dict sizes (MiB) | MB/s | compressed bytes | roundtrip |
|---|---:|---:|---:|---:|---:|---|
|1 KiB|0|614400|600.00|4607.73|147537|PASS|
|1 KiB|1|0|0.00|12290.80|36759|PASS|
|8 KiB|0|76800|600.00|9410.88|45028|PASS|
|8 KiB|1|0|0.00|13746.96|28140|PASS|
|64 KiB|0|9600|600.00|13254.40|24572|PASS|
|64 KiB|1|0|0.00|16011.10|24572|PASS|

## Interpretation and limitations
- The difference is **between two documented API contracts**, not a baseline/candidate optimization. `stableSrc=1` tells the library that source data lifetime is stable, so reuse instead of local dictionary saving is allowed. It is **not** legal to assume this for `stableSrc=0`.
- At 64 KiB, compressed bytes are equal, making this a cleaner contrast; still cannot ascribe all speed delta to copying without timing LZ4_saveDict itself and controlling compiler/cache/CPU state.
- Returned `dictSize` can be up to 64KiB, counts all bytes saved according to API; independent physical traffic / actual memcpy count may differ.
- Do not promote this to an optimization PR on current evidence. Next: construct legal context-specific copy avoidance invariant or measure LZ4_saveDict exclusive time with low-overhead controlled A/B; test unstable source mutation to ensure semantics.
