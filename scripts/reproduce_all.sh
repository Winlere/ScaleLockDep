#!/usr/bin/env bash
# Run all five reproduce scripts sequentially (overhead, cslen, latency,
# nesting, pedges). Writes 5 logs to logs/ and 5 PDFs to plots/.
#
# Usage:
#   scripts/reproduce_all.sh [SUFFIX]
#
# SUFFIX defaults to <DD>_<mon> (e.g. "06_may"). Same suffix is passed
# through to each individual script.

set -euo pipefail
cd "$(dirname "$0")/.."

SUFFIX="${1:-}"

for exp in overhead cslen latency nesting pedges; do
    echo "=================================================="
    echo "  reproduce_${exp}.sh ${SUFFIX}"
    echo "=================================================="
    ./scripts/reproduce_${exp}.sh "$SUFFIX"
done

echo
echo "All five experiments complete."
echo "  Logs:  logs/exp_*_{overhead,cslen,latency,nesting,pedges}.txt"
echo "  Plots: plots/exp_*_{overhead,cslen,latency,nesting,pedges}.pdf"
