#!/usr/bin/env bash
set -euo pipefail
cc="${1:-gcc}"
mkdir -p research-results
flags=(-O3 -std=c11 -DNDEBUG -Wall -Wextra -Werror)
src=(research/corefoundry/batch.c lib/lz4.c lib/lz4hc.c)
"$cc" "${flags[@]}" -o research-results/base "${src[@]}"
"$cc" "${flags[@]}" -DCOREF_HC8 -o research-results/hc8 "${src[@]}"
research-results/base hc > research-results/hc-base.csv
research-results/hc8 hc > research-results/hc8.csv
research-results/base oracle > research-results/oracle.csv
research-results/base delta > research-results/delta.csv
python3 research/corefoundry/summarize.py research-results
