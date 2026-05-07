#!/usr/bin/env bash
# Reproduce experiment 5: potential-graph construction cost (disjoint locks).
#
# Usage:
#   scripts/reproduce_pedges.sh [SUFFIX]
#
# Writes:
#   logs/exp_<SUFFIX>_pedges.txt
#   plots/exp_<SUFFIX>_pedges.pdf

set -euo pipefail
cd "$(dirname "$0")/.."
. scripts/_repro_common.sh

collect_metadata "${1:-}"
ensure_built

LOG="logs/exp_${SUFFIX}_pedges.txt"
PAIRS="1:64 4:64 8:32 16:16 32:8 64:4"
ITERS=100000
RUNS=3

{
    write_preamble "Potential Graph Construction Cost (Disjoint Locks)"
    cat <<EOF
--- Parameters ---

Benchmark binary : benchmarks/bench_potential_edges.out
Invocation       : bench_potential_edges.out <num_threads> <locks_per_thread> [iters]

Each thread owns a private set of <locks_per_thread> mutexes (disjoint
across threads). Each iteration acquires its locks in fixed ascending
order then releases in descending order. Application-level mutex
contention is zero; the entire baseline-to-lockdep delta is potential-
deadlock graph construction.

Sweep design: trade per-thread depth against thread count, keeping
total lock budget at or near LOCKDEP_MAX_LOCK_SLOTS (256).

  threads  depth  total_locks  total_pairs (iters=100k)
   1        64      64            6.4M
   4        64     256           25.6M
   8        32     256           25.6M
  16        16     256           25.6M
  32         8     256           25.6M
  64         4     256           25.6M

iters/thread = ${ITERS}.
${RUNS} runs per cell.

Conditions
  baseline      : no LD_PRELOAD
  lockdep/glob  : LOCKDEP_MODE=global LD_PRELOAD=./lockdep/liblockdep.so
  ScaleLockDep  : LOCKDEP_MODE=rb     LD_PRELOAD=./lockdep/liblockdep.so

Measurement
  wall_ns from clock_gettime(CLOCK_MONOTONIC) inside the binary,
  straddling pthread_create..pthread_join.

EOF
    write_lockdep_limits
    cat <<EOF
--- Raw Data ---

Columns: threads  locks_per_thread  iters  wall_ns  total_ops  ops_per_sec

EOF
    for cond in baseline global rb; do
        prefix="$(prefix_for "$cond")"
        case "$cond" in
            baseline) section="Baseline (no LD_PRELOAD)" ;;
            global)   section="lockdep - global mode" ;;
            rb)       section="lockdep - rb mode (ScaleLockDep)" ;;
        esac
        echo "$section"
        printf "threads\tlocks_per_thread\titers\twall_ns\ttotal_ops\tops_per_sec\n"
        for p in $PAIRS; do
            t=${p%:*}
            d=${p#*:}
            for _ in $(seq 1 $RUNS); do
                eval "$prefix ./benchmarks/bench_potential_edges.out $t $d $ITERS"
            done
        done
        echo
    done
} | tee "$LOG"

uv run scripts/plot_pedges.py "$LOG"
