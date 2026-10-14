# Final flush dictionary preservation probe — 2026-10-14

Branch: research/final-flush-no-save, based on research/dict-copy-probe (research-only counters; includes an earlier checksum-reset hypothesis; NOT staging-ready).

## Hypothesis
`LZ4F_compressEnd()` invokes `LZ4F_flush()` and a completed linked frame cannot need further dictionary reuse. Specialized internal flush to avoid the final `LZ4F_localSaveDict()` operation while retaining the public `LZ4F_flush()` behavior.

## Prototype
`lib/lz4frame.c`: implement `LZ4F_flush_internal(..., finishing)`; public flush passes 0, compressEnd passes 1; bypass save when finishing. Prototype includes counters used for earlier research; do not submit this combined branch.

## Test
`tests/corefoundry_final_flush_probe.c`: 5 MiB + 137 bytes generated input, 80 repeated frames/context, block size 64 KiB; independent/linked, autoFlush 0/1, chunk 1/8/64 KiB, 12 combinations. Apple Silicon, Apple Clang -O3 -DNDEBUG. Candidate and baseline compiled against same harness and different instrumented source trees. Each run checks full decompression and computes hash of compressed frame. All 12 cases roundtrip PASS and output hash/size exactly match baseline.

Linked + autoFlush 0 results per 80 frames:
- chunk 1 KiB / 8 KiB: baseline 3,200 saves, 200 MiB; prototype 3,120 saves, 195 MiB. Eliminated one 64 KiB save per frame.
- chunk 64 KiB: baseline/prototype both 6,400 saves and 400 MiB (other call path).
- other modes: unchanged saves.

## Reproducible performance regression — NO-GO
4 runs alternating A/B order demonstrate consistently slower prototype for linked, autoFlush off, chunk=1/8KiB:
- Candidate 1KiB 9,593 / 9,591 MB/s vs baseline 12,180 / 11,937 MB/s in reordered checks (~19–21% worse).
- Candidate 8KiB 9,479 / 9,330 vs baseline 11,946 / 11,646 MB/s (~19–20% worse).
- 64KiB candidate and baseline both near 13,800–14,050 MB/s, minor difference.
The cause of the regression is not yet established (could be compiler code layout, stream context state, or CPU behavior). Since byte-identical output and roundtrips pass, functional feasibility alone is insufficient. **Do not promote to Staging or claim improvement.** Next investigatory step: compare generated assembly or isolate compiler effects if warranted; otherwise return to other high-value directions.

These are developer research benchmarks, not GitHub Actions signoff or statistically controlled cross-platform benchmarks.
