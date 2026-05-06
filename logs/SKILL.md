# Experiment Methods

This document describes how to reproduce the benchmark experiments for nodeadlock.

## General Setup

### Machine & Environment

Record the following for every experiment log:

```
Machine
  OS       : uname -a
  CPU      : model, base frequency, core count, thread count
  Memory   : total RAM

Software
  Compiler : gcc (flags from benchmarks/Makefile)
  Loader   : LD_PRELOAD for lockdep injection
  Commit   : full SHA of the code under test
```

### Build

```bash
make clean && make build
```

This rebuilds `lockdep/liblockdep.so` and all benchmark binaries from scratch, ensuring the measured code matches the recorded commit.

### Conditions

Every experiment runs two conditions:

- **baseline**: benchmark binary run directly (no `LD_PRELOAD`)
- **lockdep**: benchmark binary run with `LD_PRELOAD=./lockdep/liblockdep.so`

Record the lockdep limits from `lockdep/lockdep.h` (`LOCKDEP_MAX_LOCK_SLOTS`, `LOCKDEP_MAX_HELD_LOCK_SLOTS`, `LOCKDEP_MAX_THREAD_SLOTS`).

### Parameters

Default values used across experiments (configurable in `Makefile`):

| Parameter        | Value                             |
|------------------|-----------------------------------|
| Thread counts    | 1, 2, 4, 8, 16, 32, 64           |
| Iterations       | 100,000 per thread                |
| CS hold times    | 0–100,000 ns (cslen experiment)   |
| CS threads       | 8 (cslen experiment)              |

---

## Experiments

### 1. Overhead — High/Low Contention

**Make target**: `make overhead`
**Binary**: `benchmarks/bench_overhead.out`
**Invocation**: `bench_overhead.out <num_threads> <num_locks> <iters_per_thread>`

Two scenarios:
- **High contention**: `num_locks = 1` — all threads share one lock. Measures lockdep overhead when the application is already serialized.
- **Low contention**: `num_locks = num_threads` — each thread has its own lock. Isolates pure lockdep tracking overhead from application synchronization.

**Metric**: `ops_per_sec` (total lock/unlock pairs / wall time)
**Overhead**: baseline ops/s ÷ lockdep ops/s


### 3. Overhead — Critical Section Length

**Make target**: `make overhead-cslen`
**Binary**: `benchmarks/bench_overhead_cslen.out`
**Invocation**: `bench_overhead_cslen.out <num_threads> <iters> <cs_ns>`

1 shared lock, 8 threads (fixed). Each iteration: lock, busy-spin for `<cs_ns>` nanoseconds, unlock. The busy-spin is calibrated at startup via a trial loop.

CS hold times swept: 0, 10, 20, 30, 50, 75, 100, 150, 200, 300, 500, 750, 1000, 10000, 100000 ns.

**Purpose**: Determines the critical section length at which lockdep overhead becomes negligible. Identifies the crossover point where application hold time exceeds lockdep metadata cost.

**Metric**: `ops_per_sec`

### 4. Per-Operation Latency

**Make target**: `make latency`
**Binary**: `benchmarks/bench_latency.out`
**Invocation**: `bench_latency.out <num_threads> <iters_per_thread>`

1 shared lock, sweep thread counts. Each iteration individually times the lock and unlock calls using `clock_gettime(CLOCK_MONOTONIC)`. Reports per-thread averages.

**Note**: `clock_gettime` adds ~15–25 ns measurement overhead per call. This is consistent across conditions, so the *difference* between baseline and lockdep is accurate.

**Metrics**: `avg_lock_ns`, `avg_unlock_ns`, `avg_pair_ns`

---

## Log File Format

Each experiment log follows this structure:

```
Experiment: <title>
Date: <YYYY-MM-DD>
Commit ID: <short SHA>

--- Settings ---
<machine, software, commit>

--- Parameters ---
<binary, invocation, thread counts, iterations, scenario description>

--- Raw Data ---
<TSV tables for baseline and lockdep>

--- Key Findings ---
<summary table with overhead ratios>

--- Comparison with <previous experiment> ---
<side-by-side table if re-running a prior experiment>

--- What the Experiment Reveals ---
<numbered observations>
```

### Naming Convention

```
exp_<DD>_<mon>[_<variant>].txt
```

Examples:
- `exp_18_mar.txt` — overhead (high/low contention), March 18
- `exp_24_mar_cslen.txt` — cslen variant, March 24
- `exp_20_apr_latency.txt` — latency variant, April 20

---

## Plotting

Python scripts in `scripts/` generate PDF plots from the raw data:

| Script                      | Experiment               | Output                    |
|-----------------------------|--------------------------|---------------------------|
| `plot_overhead.py`          | High/low contention      | `plots/overhead_benchmarks.pdf` |
| `plot_overhead-cslen.py`    | Critical section length  | `plots/exp_24_mar_cslen.pdf`    |
| `plot_latency.py`           | Per-operation latency    | `plots/exp_24_mar_latency.pdf`  |
