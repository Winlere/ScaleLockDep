#!/usr/bin/env bash
# Reproduce experiment 1: overhead (high/low contention).
#
# Usage:
#   scripts/reproduce_overhead.sh [SUFFIX]
#
# Writes:
#   logs/exp_<SUFFIX>_overhead.txt
#   plots/exp_<SUFFIX>_overhead.pdf
#
# SUFFIX defaults to <DD>_<mon> (e.g. "06_may").

set -euo pipefail
cd "$(dirname "$0")/.."
. scripts/_repro_common.sh

collect_metadata "${1:-}"
ensure_built

LOG="logs/exp_${SUFFIX}_overhead.txt"
THREAD_COUNTS="1 2 4 8 16 32 64"
ITERS=100000

{
    write_preamble "Overhead Microbenchmarks (High/Low Contention)"
    cat <<EOF
--- Parameters ---

Benchmark binary : benchmarks/bench_overhead.out
Invocation       : bench_overhead.out <num_threads> <num_locks> <iters_per_thread>
Thread counts    : ${THREAD_COUNTS}
Iterations       : ${ITERS} per thread
Metric           : ops_per_sec  (total lock/unlock pairs / wall time)

Scenarios
  high-contention : num_locks = 1           (all threads share one lock)
  low-contention  : num_locks = num_threads (each thread owns its own lock,
                                             isolates lockdep tracking overhead
                                             from application synchronization)

Conditions per scenario
  baseline      : no LD_PRELOAD
  lockdep/glob  : LOCKDEP_MODE=global LD_PRELOAD=./lockdep/liblockdep.so
  ScaleLockDep  : LOCKDEP_MODE=rb     LD_PRELOAD=./lockdep/liblockdep.so

EOF
    write_lockdep_limits
    cat <<EOF
--- Raw Data ---

EOF
    for scenario in high low; do
        if [ "$scenario" = "high" ]; then
            scen_label="High-contention (1 shared lock)"
        else
            scen_label="Low-contention (locks = threads)"
        fi
        for cond in baseline global rb; do
            prefix="$(prefix_for "$cond")"
            echo "${scen_label} - $(label_for "$cond")"
            printf "threads\tlocks\titers\twall_ns\ttotal_ops\tops_per_sec\n"
            for t in $THREAD_COUNTS; do
                if [ "$scenario" = "high" ]; then
                    locks=1
                else
                    locks=$t
                fi
                eval "$prefix ./benchmarks/bench_overhead.out $t $locks $ITERS"
            done
            echo
        done
    done
} | tee "$LOG"

uv run scripts/plot_overhead.py "$LOG"
