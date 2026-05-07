#!/usr/bin/env bash
# Reproduce experiment 3: per-operation latency.
#
# Usage:
#   scripts/reproduce_latency.sh [SUFFIX]
#
# Writes:
#   logs/exp_<SUFFIX>_latency.txt
#   plots/exp_<SUFFIX>_latency.pdf

set -euo pipefail
cd "$(dirname "$0")/.."
. scripts/_repro_common.sh

collect_metadata "${1:-}"
ensure_built

LOG="logs/exp_${SUFFIX}_latency.txt"
THREAD_COUNTS="1 2 4 8 16 32 64"
ITERS=100000

{
    write_preamble "Per-Operation Latency - Lock Acquire and Release Delay"
    cat <<EOF
--- Parameters ---

Benchmark binary : benchmarks/bench_latency.out
Invocation       : bench_latency.out <num_threads> <iters_per_thread>
Thread counts    : ${THREAD_COUNTS}
Iterations       : ${ITERS} per thread
Locks            : 1 shared lock
Metric           : average nanoseconds per operation (lock, unlock, pair)

Scenario
  1 shared lock, varying thread counts. Each iteration: time lock, time
  unlock, accumulate separately. Reports per-thread averages aggregated
  across all threads.

  Note: clock_gettime(CLOCK_MONOTONIC) adds ~15-25 ns of measurement
  overhead per call. This overhead is consistent across baseline and
  lockdep, so the *difference* between conditions is accurate. Absolute
  values include this measurement cost.

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
        echo "$(label_for "$cond")"
        printf "threads\titers\tavg_lock_ns\tavg_unlock_ns\tavg_pair_ns\n"
        for t in $THREAD_COUNTS; do
            eval "$prefix ./benchmarks/bench_latency.out $t $ITERS"
        done
        echo
    done
} | tee "$LOG"

uv run scripts/plot_latency.py "$LOG"
