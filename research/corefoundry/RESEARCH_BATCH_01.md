# CoreFoundry — LZ4 opportunity gate, batch 01 (2026-10-14)

## Isolation / constraints
- Independent non-fork Research repository `Johnny-Kao/lz4-research`, branch `research/batch-opportunity-gate`.
- Base includes upstream `lz4/lz4` dev at `83376b66` (2026-10-14) and the research CI guard commit.
- Github-hosted standard public-repository runners only; no private OSS Toolkit CI; no personal M5; no paid runners; one manually-dispatched job matrix (Linux x64 GCC, x64 Clang, Linux arm64 GCC). All inherited upstream CI workflows remain disabled.
- No new format or new feature upstream; no PR, no staging transfer.

## Upstream overlap audit
- 2026-10-14: PR #1848 merged: HC stop searching at theoretical maximal match length; #1852 merged: simplify HC hash-chain parser; #1855 merged: favorDecSpeed optimal parser preference; #1840 merged: ring-buffer handling.
- 2026-10-09: PR #1829 merged: reused-context small-block compression speedup; PR #1830 closed without merge (small-frame init alternate); #1790 open (redundant state init); #1784 open (multithreaded CLI reads); issue #1818 (multithreaded compression in liblz4).
- Official `NEWS` (v1.10.1 development) emphasizes HC, frames, decoding and context reuse, so do NOT repeat these proposals.
- No evidence of an authoritative public dated feature roadmap was found in README/NEWS; treat these sources as activity evidence, not a committed roadmap.

## Three independent experimental families
A. **LZ4HC 8-byte reverse compare vs upstream 4-byte compare**, compile-time `COREF_HC8` only, no CPU probing, on supported little-endian GCC/Clang; tests levels 3/9/12 × 5 synthetic distributions. Compression bytes and FNV fingerprint MUST match for every pair. This is a source-compatible core optimization if genuine speedup proves stable.
B. **Sparse delta representation opportunity**: two 4 KiB pages with 0/1/8/64/512/2048/4096 mutations, 3 base families. Encode first 4 KiB verbatim plus 16-bit count and (16-bit index + new byte) per changed position; compare size and encode time with ordinary LZ4 compression of the same contiguous 8 KiB source. FULL roundtrip. This delta format is *NOT LZ4-compatible*; scores do not justify upstream feature PR.
C. **Post hoc Oracle acceleration 1/4/16** for five data distributions and effective bandwidth 100/1000 MB/s, using measured CPU and compressed size. This tests the size of the opportunity, not a deployable predictor. Does not add a threshold or CPU detector.

## Important limits
- Generated corpora are synthetic, not representative; warm caches and shared runners introduce noise. For actionability require repeated randomized interleaved trials and real workloads later.
- Delta study includes raw reference page in size and encoder time but not decode-time cost; a pipeline claim requires decoder and reference-lifetime pricing.
- HC correctness includes baseline/candidate output byte-identical plus successful decompression. No claims of wins before GitHub Actions results.
- No fixed entropy or Q-like heuristic accepted; no GPU/CPU state detection.
- Potentially safe SIMD is only an implementation acceleration of a proven algorithm, not an architecture-dependent policy.

## Execution and records
`gh workflow run corefoundry-research-batch.yml -R Johnny-Kao/lz4-research --ref research/batch-opportunity-gate`.
Save permanent GitHub Actions run links and uploaded logs. If any compiled variant fails, record blocker transparently and correct the harness in Research only before a new run.
