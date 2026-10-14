# Adaptive compressibility / matchability feasibility — Apple M5 — 2026-10-14

Repository stage: independent Research only. Branch: research/adaptive-matchability, base exact upstream dev commit 4100eec1475f6eac351c5d5a6cf30db8a2c4a1ba. This experiment has NO modifications to lz4 library code, staging, or upstream.

## Hypothesis
Compression utility depends on locality and repetition (LZ matchability), compressor CPU time, output size, and downstream transfer/decode economics, rather than a single universal compression policy. A cheap predictor might dynamically select LZ4 acceleration (or independent-frame raw-block mode) before spending heavy CPU on poorly compressible data.

## What upstream already implements
- lz4.c: main match finder uses increasing search step (LZ4_skipTrigger=6, searchMatchNb accumulator); it does not simply scan every byte when matches are absent.
- lz4frame.c: LZ4F_makeBlock first attempts compression, then stores the block uncompressed when compression would not save space; skip before compression would be different, but would need to preserve linked dictionary state.
- LZ4_compress_fast acceleration API already allows trading match density for speed.
- Frame-level LZ4F_uncompressedUpdate is available but has independent-block limitations. Do not directly bypass linked-block compression without addressing dictionary state.

## Initial pilot: static sources and a deliberately naive sampler
Harness: tests/corefoundry_adaptive_probe.c. Apple M5 arm64, Apple Clang -O3 -DNDEBUG; 4 KiB, 64 KiB and 256 KiB fixed input sizes; sources: repeated text, pseudorandom, repeated high-entropy 4 KiB tile, half random/half repeated text, mostly structured bytes with occasional noise. Compares acceleration 1, 4, 16, and an adaptive choice based on 64 sampled 4-byte words / collision-repeat count. Compression throughput includes repeated per-call LZ4_compress_fast; adaptive mode includes sample CPU work. Every row performs decompress and byte-match correctness. Single runs, fully cache-warm synthetic workload; throughput on M5 cannot be transferred to other CPUs, application I/O, or larger input corpora.

Selected 256 KiB results (source size fraction and MiB/s):
| Source | Sample repeats | accel 1 ratio / MiB/s | accel 16 ratio / MiB/s | naive adaptive |
|---|---:|---|---|---|
| pseudorandom | 0 | 1.0039 / 34079 | 1.0039 / 36819 | 16 |
| repeated random 4 KiB tile | 0 | 0.0196 / 31624 | 0.0196 / 32442 | 16 |
| repeated text | 12 | 0.0042 / 32586 | 0.0051 / 33130 | 1 |
| mixed random/repeated | 5 | 0.5101 / 30257 | 0.5103 / 32255 | 4 |
| structured with noise | 0 | 0.2925 / 1754 | 0.3101 / 1894 | 16 |

Selected 4 KiB result: structured accel1 ratio 0.3347 at 1801 MiB/s; accel16 ratio 0.5271 at 9439 MiB/s, and naive sampler predicts 16. Large compression ratio deterioration demonstrates an unacceptable speed/size trade-off for some workloads. Note source generation is synthetic, not representative of all applications.

## Byte entropy cross-check (second exploratory run)
For 256 KiB input: generated pseudorandom content has measured 7.9992 bits/byte and compresses to ~100.39% of original; repeating one generated random 4 KiB tile has measured 7.9541 bits/byte but compresses to ~1.96%. The naive word probe reports zero hits for both. Highly similar first-order byte entropy does not imply similar sequence compressibility. The 4 KiB tile is intentionally constructed, not real workload evidence.

## Key result
**Entropy or sparse exact-word sampling is not a reliable proxy for LZ matchability.** Pure random and a 4 KiB high-entropy repeated tile both register 0 matches in this pilot's simplistic 64-position word sampler at size 256 KiB, yet have opposite compressibility (ratio ~1.004 vs 0.020). A naive adaptive detector can sacrifice substantial ratio on structured data. No new algorithm or upstream-credible gain was established.

## Priority research approach
1. Measure *total utility* including prediction overhead, compression CPU, compressed bytes / effective storage/network bandwidth, decode CPU. Compression wins should remain robust across relevant bandwidth limits, not just raw MB/s.
2. Favor feedback from already-computed matching/compression information (low overhead, no second scan) and local chunk transitions over a global Shannon entropy threshold.
3. Preserve linked-block decoder/history invariants. For prototype policy, start with independent blocks and per-block acceleration changes. Avoid early raw skip until dictionary semantics audited.
4. Benchmark realistic corpus families (code/text, JSON, log, database pages, media already compressed, high-entropy repeated records) and intentionally adversarial transitions. Include warm-up/cold-start and large memory runs, cross CPU tests, multiple randomized A/B passes, compressed output correctness.
5. No GO until repeatable pipeline-level gains with controlled compression-size regression. Use narrow workflow_dispatch-only Actions in Research repo for reproducible cross-platform gates if needed.

Status: IDEA VALIDATED AS ECONOMIC HYPOTHESIS, NAIVE SAMPLER NO-GO, proper adaptive algorithm unproven.
