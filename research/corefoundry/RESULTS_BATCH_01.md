# CoreFoundry — LZ4 Batch 01 Results (2026-10-14)

## Execution links and provenance
- **Corrected, authoritative run:** https://github.com/Johnny-Kao/lz4-research/actions/runs/38546776853
- First pilot run: https://github.com/Johnny-Kao/lz4-research/actions/runs/38546411510 (all three jobs succeeded, but DELTA *timings must not be used*: original prototype knew mutation positions in advance and did not pay the detection scan).
- Research branch: `research/batch-opportunity-gate`; corrected source commit `7c47bd7c`.
- Matrix: ubuntu-24.04 Linux x86-64 GCC; ubuntu-24.04 Linux x86-64 Clang; ubuntu-24.04-arm Linux ARM64 GCC. All three jobs completed SUCCESS and uploaded CSV reports as GitHub Actions artifacts.
- **No tests ran on the user's M5**; no private OSS Toolkit CI; no upstream or Staging PR. Compiler, host, and code are available in the linked run logs.

## Scheme A — HC reverse match extension (4-byte baseline vs 8-byte)
Identical compressed length and FNV of *all 15 synthetic cases* for baseline and candidate on each job; each output roundtrips via LZ4_decompress_safe. No wire format changes. 3 HC levels (3, 9, 12), 5 source families, 24 iterations × 3 measurements; median runtime per call.

| Matrix | Candidate wins / 15 | Median gain | Worst measured regression |
|---|---:|---:|---:|
| Linux ARM64 GCC | 10 | +1.07% | -3.17% |
| Linux x64 Clang | 10 | +2.18% | -25.54% |
| Linux x64 GCC | 4 | -1.17% | -24.27% |

**NO-GO.** While it preserves compressed bytes in tested cases, it has compiler-dependent performance and large negative tails. Not enough statistical evidence, and no implementation invariant guarantees CPU gains. This is not ready for an upstream PR.

## Scheme B — Independent Sparse Delta format
Input: two contiguous 4KiB pages; reference page stored verbatim, followed by a 16-bit mutation count and a 16-bit changed position + 8-bit new value for each edit. Encoder scans every byte of both pages; decoder reconstructs full 8KiB. Compared against standard one-shot LZ4 compression/decompression of precisely the same 8KiB input. Sizes below are exact and common across platforms.

| Mutations in pseudo-random page | Standard LZ4 bytes | Delta bytes | Delta size saving |
|---:|---:|---:|---:|
| 0 | 4138 | 4098 | +0.97% |
| 1 | 4142 | 4101 | +0.99% |
| 8 | 4175 | 4122 | +1.27% |
| 64 | 4447 | 4290 | +3.53% |
| 512 | 5063 | 5634 | -11.28% |
| 2048 | 6872 | 10242 | -49.04% |
| 4096 | 8226 | 16386 | -99.20% |

Across all three 7-case data families (random-base, text-base, structured-base), delta is smaller in 4/21 cases, all from the random-base family with 0–64 edits. For text-base and structured-base LZ4 massively out-compresses the uncompressed reference approach. Delta runtime varies greatly across compilers; on ARM64 GCC, even the 0-edit delta encoder was slower than LZ4 encoder (2491.5 ns vs 1178.4 ns), while decoder was faster (94.6 ns vs 204.5 ns). On x64 Clang 64-edit random-base, delta encoding was faster (1448.7 ns vs 2550.0 ns) but is different/unstandardized format.

**RESEARCH-ONLY, NOT AN UPSTREAM LZ4 FORMAT IMPROVEMENT.** Fundamental limitations: reference stored raw, encoder and decoder in bespoke test, no standalone validation/metadata/error handling, and standard LZ4 cannot decode. If pursued, optimize as an independent transform against an already-available shared reference, with complete format and roundtrip cost.

## Scheme C — Post-hoc acceleration Oracle (not a deployed adaptive algorithm)
For five synthetic 64 KiB datasets, compare fixed LZ4_fast acceleration 1/4/16, include compression CPU and compressed-byte transfer modeled at 100 MB/s and 1000 MB/s. Oracle observes every candidate's result after the fact; therefore the computation excludes any decision overhead and measures **a restricted upper bound** among these three choices, not a universal optimum.

Representative maximum across data families:
- On Linux ARM64 GCC: at 100 MB/s, best improvement over accel1 +1.11% (repeated random tile); at 1000 MB/s, +6.57% (same).
- On Linux x64 GCC: at 100 MB/s, +1.09%; at 1000 MB/s, +6.46%.
- On Linux x64 Clang: at 100 MB/s, +0.88% (repeated tile; text +0.81%); at 1000 MB/s, +14.00% in text (test artifact particularly sensitive to tiny µs measurements); repeated tile +5.22%.
- Many sources are at 0–2% gains, and these are warm-cache synthetic blocks rather than pipelines.

**NO-GO** on a new policy/controller: available gains on these workloads are too small or noisy relative to cost of a maintained decision mechanism. Real corpus may give a different answer; this does not establish a general mathematical impossibility.

## Upstream alignment (Oct 14, 2026)
Latest upstream dev was synchronized before experiment. `NEWS` v1.10.1 development records fast context reuse (up to +94%), HC 11/12, ARM64 decoder changes, new frame/seek APIs; recent PRs #1848 (HC maximum-length stop), #1852 (HC hash-chain parser), #1855 (decoding speed preference), #1840 (ring buffer) are merged. Open issue #1818 requests liblz4 multithreading; PR #1784 handles threaded CLI chunk reads. None of these are equivalent to a delta sidecar, but compatibility and format complexity make the latter inappropriate for the existing standard without a proven benefit. Official README/NEWS did not expose an authoritative dated roadmap; do not invent one.

## Next gate
1. **Stop HC8** as a PR candidate unless a source-level reason and cross-compiler evidence overcomes regressive cases; do not tune CPU thresholds.
2. **Stop naive Sparse Delta** as universal compression; if exploring, restrict math problem to *reference already retained and shared* with exact source/version ownership, compare both transforms' end-to-end costs. The partial gains without a retained reference are too narrow.
3. **Stop ad-hoc adaptive acceleration** unless a theoretical upper bound on real distributions justifies its added state and decision complexity.
4. For future batch, choose non-overlapping upstream gap based on proof of redundant work, sample realistic repositories/logs/datasets, and predefine target regression budget. Require randomized same-run A/B, multiple fresh runs and independently verified correctness before Staging.

**None of the 3 schemes currently clears CoreFoundry's Research → Staging gate.**
