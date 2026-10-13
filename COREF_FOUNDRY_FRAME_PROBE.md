# Initial Frame/Dictionary probe — 2026-10-14 JST

## Scope and repository
Independent repository `Johnny-Kao/lz4-research` (non-fork), primary upstream source commit `4100eec1475f6eac351c5d5a6cf30db8a2c4a1ba`. All research code and experiments belong here. No change was transferred to Staging or upstream.

## CI maintenance
- Disabled ten inherited workflows temporarily through GitHub workflow disable controls: Build Systems, CMake Build Tests, Code Quality, Compiler Tests, Core Tests, Cross Platform, OSS-Fuzz, Release & Environment, Sanitizers, Scorecard.
- Original independent repo had Dependabot github-actions updates enabled. Research-repo dev includes commit `0c21f450`, `.github/dependabot.yaml` `open-pull-requests-limit: 0` to stop further update PRs; Dependabot Updates pseudo-workflow itself cannot be manually disabled via `gh workflow disable`.
- Existing failure [OSS-Fuzz run 38476331686](https://github.com/Johnny-Kao/lz4-research/actions/runs/38476331686) fails in CIFuzz build step: `Failed to detect repo`. Treat as integration/provenance setup, not a validated candidate defect.
- Existing failure [Cross Platform run 38476331503](https://github.com/Johnny-Kao/lz4-research/actions/runs/38476331503) includes macOS `test-fuzzer` mismatch: `LZ4_compress_fast_continue using extDictCtx produced different-sized output (52651 expected vs 52804 actual) (seed 6537, cycle 5622)`. Not yet isolated baseline vs candidate; do not dismiss as CI-only.

## Local Apple Silicon frame probe
Harness: `tests/corefoundry_frame_probe.c`, compiled with Apple Clang `-O3 -DNDEBUG` against four source files from research candidate and pristine baseline. 1 MiB generated input, 600 frame compressions per case, linked-block, autoFlush true, chunk sizes 1/8/64 KiB, stableSrc=0 or =1, reuse one compression context. Verify latest frame by LZ4F_decompress and byte-compare original (all six cases pass for baseline and candidate). This is an initial single-run pilot, not randomized/interleaved or confidence-interval measurement.

Candidate speeds in MB/s:
| Chunk | stableSrc 0 | stableSrc 1 |
| --- | ---: | ---: |
| 1 KiB | 4645.90 | 12372.66 |
| 8 KiB | 8712.19 | 13906.59 |
| 64 KiB | 13270.81 | 16183.41 |

Baseline speeds in MB/s:
| Chunk | stableSrc 0 | stableSrc 1 |
| --- | ---: | ---: |
| 1 KiB | 4604.02 | 12321.34 |
| 8 KiB | 9389.38 | 13783.60 |
| 64 KiB | 13294.04 | 16079.32 |

Compressed frame byte counts were identical candidate/baseline at corresponding settings but DIFFERENT across stableSrc values at 1/8 KiB; both stream forms round-trip correctly. Consequently differences between stableSrc 0 and stableSrc 1 cannot yet be attributed specifically to dictionary-copy savings (compression work and ratio differ). No identifiable checksum-reset win.

## Gate
- A1 checksum reset: NO-GO as performance PR on current evidence.
- Ownership/dictionary: INVESTIGATE with controlled instrumentation and same-output/ratio normalization. Source-copy reductions must be shown to preserve contractual semantics.
- Next measurement: count bytes copied by LZ4F_localSaveDict, compare stableSrc modes at identical compression output/algorithmic work where possible; test nonstable source lifetime separately; expand to multiple datasets.
- Do not run inherited workflow matrices. When ready, add narrow workflow_dispatch-only experiment, then deliberately enable that single workflow (all inherited workflows remain disabled).
