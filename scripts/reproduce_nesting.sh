#!/usr/bin/env bash
# Reproduce experiment 4: nesting depth scaling (3-deep vs 40-deep, 5 runs/cell).
#
# Usage:
#   scripts/reproduce_nesting.sh [SUFFIX]
#
# Writes:
#   logs/exp_<SUFFIX>_nesting.txt
#   plots/exp_<SUFFIX>_nesting.pdf

set -euo pipefail
cd "$(dirname "$0")/.."
. scripts/_repro_common.sh

collect_metadata "${1:-}"
ensure_built

LOG="logs/exp_${SUFFIX}_nesting.txt"
SHALLOW=./benchmarks/correct_40threads_3locks_10000iter.out
DEEP=./benchmarks/correct_40threads_40locks_10000iter.out
RUNS=5

run_cell() {
    local bin="$1" cond="$2"
    local prefix
    prefix="$(prefix_for "$cond")"
    local t1 t2
    t1=$(date +%s%N)
    eval "$prefix $bin >/dev/null"
    t2=$(date +%s%N)
    echo "$((t2 - t1))"
}

{
    write_preamble "Nesting Depth Scaling (3-deep vs 40-deep)"
    cat <<EOF
--- Parameters ---

Benchmark binaries
  shallow : ${SHALLOW}
  deep    : ${DEEP}

Both binaries are parameter-free. Each iteration acquires N locks in a
fixed global order, then releases in reverse. No actual deadlock; this
exercises potential-deadlock graph maintenance under deep held-stack
pressure.

Workload
  threads        = 40
  iters/thread   = 10,000
  shallow N      = 3   ->  pairs = 40 x 10000 x 3  = 1,200,000
  deep    N      = 40  ->  pairs = 40 x 10000 x 40 = 16,000,000

Conditions
  baseline      : no LD_PRELOAD
  lockdep/glob  : LOCKDEP_MODE=global LD_PRELOAD=./lockdep/liblockdep.so
  ScaleLockDep  : LOCKDEP_MODE=rb     LD_PRELOAD=./lockdep/liblockdep.so

Measurement
  wall_ns captured externally with date +%s%N straddling the run.
  Stdout redirected to /dev/null (per-iter progress printf is much
  noisier than the work being measured).
  ${RUNS} runs per cell.

EOF
    write_lockdep_limits
    cat <<EOF
--- Raw Data ---

Shallow (3-deep nest, 3 shared locks, 1,200,000 pairs/run)
condition  run  wall_ns
EOF
    for cond in baseline global rb; do
        for r in $(seq 1 $RUNS); do
            wall=$(run_cell "$SHALLOW" "$cond")
            printf "%-9s  %d  %d\n" "$cond" "$r" "$wall"
        done
    done

    cat <<EOF

Deep (40-deep nest, 40 shared locks, 16,000,000 pairs/run)
condition  run  wall_ns
EOF
    for cond in baseline global rb; do
        for r in $(seq 1 $RUNS); do
            wall=$(run_cell "$DEEP" "$cond")
            printf "%-9s  %d  %d\n" "$cond" "$r" "$wall"
        done
    done
    echo
} | tee "$LOG"

uv run scripts/plot_nesting.py "$LOG"
