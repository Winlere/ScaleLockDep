#!/usr/bin/env bash
# Reproduce experiment 2: overhead vs critical section length.
#
# Usage:
#   scripts/reproduce_cslen.sh [SUFFIX]
#
# Writes:
#   logs/exp_<SUFFIX>_cslen.txt
#   plots/exp_<SUFFIX>_cslen.pdf

set -euo pipefail
cd "$(dirname "$0")/.."
. scripts/_repro_common.sh

collect_metadata "${1:-}"
ensure_built

LOG="logs/exp_${SUFFIX}_cslen.txt"
CSLEN_THREADS=8
ITERS=100000
CS_LENGTHS="0 10 20 30 50 75 100 150 200 300 500 750 1000 10000 100000"

{
    write_preamble "Overhead Microbenchmarks - Critical Section Length"
    cat <<EOF
--- Parameters ---

Benchmark binary : benchmarks/bench_overhead_cslen.out
Invocation       : bench_overhead_cslen.out <num_threads> <iters> <cs_ns>
Threads          : ${CSLEN_THREADS} (fixed)
Locks            : 1 shared lock
Iterations       : ${ITERS} per thread
CS hold times    : ${CS_LENGTHS}
Metric           : ops_per_sec  (total lock acquisitions / wall time)

Scenario
  1 shared lock, 8 threads. Each iteration: lock, busy-spin for <cs_ns>
  nanoseconds inside the critical section, unlock. The busy-spin is
  calibrated at startup via a trial loop.

Conditions
  baseline      : no LD_PRELOAD
  lockdep/glob  : LOCKDEP_MODE=global LD_PRELOAD=./lockdep/liblockdep.so
  ScaleLockDep  : LOCKDEP_MODE=rb     LD_PRELOAD=./lockdep/liblockdep.so

EOF
    write_lockdep_limits
    cat <<EOF
--- Raw Data ---

EOF
    for cond in baseline global rb; do
        prefix="$(prefix_for "$cond")"
        echo "Critical section length - $(label_for "$cond")"
        printf "threads\titers\tcs_ns\twall_ns\ttotal_ops\tops_per_sec\n"
        for cs in $CS_LENGTHS; do
            eval "$prefix ./benchmarks/bench_overhead_cslen.out $CSLEN_THREADS $ITERS $cs"
        done
        echo
    done
} | tee "$LOG"

uv run scripts/plot_overhead-cslen.py "$LOG"
